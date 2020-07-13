package sjtu.sdic.mapreduce.core;

import com.alibaba.fastjson.JSON;
import sjtu.sdic.mapreduce.common.KeyValue;
import sjtu.sdic.mapreduce.common.Utils;

import java.nio.file.Paths;
import java.util.*;
import java.util.stream.Collectors;
import java.util.stream.IntStream;
import java.util.stream.Stream;

/**
 * Created by Cachhe on 2019/4/19.
 */
public class Reducer {

    /**
     * 
     * 	doReduce manages one reduce task: it should read the intermediate
     * 	files for the task, sort the intermediate key/value pairs by key,
     * 	call the user-defined reduce function {@code reduceF} for each key,
     * 	and write reduceF's output to disk.
     *
     * 	You'll need to read one intermediate file from each map task;
     * 	{@code reduceName(jobName, m, reduceTask)} yields the file
     * 	name from map task m.
     *
     * 	Your {@code doMap()} encoded the key/value pairs in the intermediate
     * 	files, so you will need to decode them. If you used JSON, you can refer
     * 	to related docs to know how to decode.
     * 	
     *  In the original paper, sorting is optional but helpful. Here you are
     *  also required to do sorting. Lib is allowed.
     * 	
     * 	{@code reduceF()} is the application's reduce function. You should
     * 	call it once per distinct key, with a slice of all the values
     * 	for that key. {@code reduceF()} returns the reduced value for that
     * 	key.
     * 	
     * 	You should write the reduce output as JSON encoded KeyValue
     * 	objects to the file named outFile. We require you to use JSON
     * 	because that is what the merger than combines the output
     * 	from all the reduce tasks expects. There is nothing special about
     * 	JSON -- it is just the marshalling format we chose to use.
     * 	
     * 	Your code here (Part I).
     * 	
     * 	
     * @param jobName the name of the whole MapReduce job
     * @param reduceTask which reduce task this is
     * @param outFile write the output here
     * @param nMap the number of map tasks that were run ("M" in the paper)
     * @param reduceF user-defined reduce function
     */
    public static void doReduce(String jobName, int reduceTask, String outFile, int nMap, ReduceFunc reduceF) {
        Stream<List<KeyValue>> MapResults= IntStream.range(0, nMap)
                .mapToObj(m -> JSON.parseArray(Mapper.readFile(Paths.get(Utils.reduceName(jobName, m, reduceTask)),true), KeyValue.class));
        Map<String,List<KeyValue>> MapFlatResults = MapResults.flatMap(Collection::stream).collect(Collectors.groupingBy(kv -> kv.key));
        Map<String,String> res = new TreeMap<>();
        for(Map.Entry<String,List<KeyValue>> entry : MapFlatResults.entrySet()){
            res.put(entry.getKey(),reduceF.reduce(entry.getKey(),entry.getValue().stream().map(kv -> kv.value).toArray(String[]::new)));
        }
        Mapper.writeFile(Paths.get(outFile),JSON.toJSONString(res));

    }

}
