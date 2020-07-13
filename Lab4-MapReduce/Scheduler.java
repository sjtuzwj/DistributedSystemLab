package sjtu.sdic.mapreduce.core;


import com.alipay.sofa.rpc.core.exception.SofaRpcException;
import sjtu.sdic.mapreduce.common.Channel;
import sjtu.sdic.mapreduce.common.DoTaskArgs;
import sjtu.sdic.mapreduce.common.JobPhase;
import sjtu.sdic.mapreduce.common.Utils;
import sjtu.sdic.mapreduce.rpc.Call;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.stream.IntStream;


/**
 * Created by Cachhe on 2019/4/22.
 */
public class Scheduler {

    /**
     * schedule() starts and waits for all tasks in the given phase (mapPhase
     * or reducePhase). the mapFiles argument holds the names of the files that
     * are the inputs to the map phase, one per map task. nReduce is the
     * number of reduce tasks. the registerChan argument yields a stream
     * of registered workers; each item is the worker's RPC address,
     * suitable for passing to {@link Call}. registerChan will yield all
     * existing registered workers (if any) and new ones as they register.
     *
     * @param jobName job name
     * @param mapFiles files' name (if in same dir, it's also the files' path)
     * @param nReduce the number of reduce task that will be run ("R" in the paper)
     * @param phase MAP or REDUCE
     * @param registerChan register info channel
     */
    public static void schedule(String jobName, String[] mapFiles, int nReduce, JobPhase phase, Channel<String> registerChan) {
        int nTasks = -1; // number of map or reduce tasks
        int nOther = -1; // number of inputs (for reduce) or outputs (for map)
        switch (phase) {
            case MAP_PHASE:
                nTasks = mapFiles.length;
                nOther = nReduce;
                break;
            case REDUCE_PHASE:
                nTasks = nReduce;
                nOther = mapFiles.length;
                break;
        }

        System.out.println(String.format("Schedule: %d %s tasks (%d I/Os)", nTasks, phase, nOther));

        /**
        // All ntasks tasks have to be scheduled on workers. Once all tasks
        // have completed successfully, schedule() should return.
        //
        // Your code here (Part III, Part IV).
        //
        */
            try {
            CountDownLatch latch = new CountDownLatch(nTasks);
            for(int i = 0; i<nTasks;i++){
                new WorkerThread(registerChan,new DoTaskArgs(jobName, mapFiles[i], phase, i, nOther),latch).start();
            }
            latch.await();
        } catch (InterruptedException e) {
            e.printStackTrace();
        }

        System.out.println(String.format("Schedule: %s done", phase));

    }
    private static class WorkerThread extends Thread {
        final Channel<String> workers;
        final DoTaskArgs args;
        final CountDownLatch latch;

        public WorkerThread(Channel<String> workers, DoTaskArgs args, CountDownLatch latch) {
            this.workers = workers;
            this.args = args;
            this.latch = latch;
        }

        @Override
        public void run() {
                try {
                    String worker = workers.read();
                    Call.getWorkerRpcService(worker).doTask(args);
                    workers.write(worker);
                    latch.countDown();
                } catch (InterruptedException e) {
                    Utils.debug("worker thread interrupted");
                    new WorkerThread(workers,args,latch).start();
                } catch (SofaRpcException e) {
                    Utils.debug("worker failed");
                    new WorkerThread(workers,args,latch).start();
                }
            }
    }
}
