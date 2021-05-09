package mapreduce

import (
	"fmt"
)

//
// schedule() starts and waits for all tasks in the given phase (mapPhase
// or reducePhase). the mapFiles argument holds the names of the files that
// are the inputs to the map phase, one per map task. nReduce is the
// number of reduce tasks. the registerChan argument yields a stream
// of registered workers; each item is the worker's RPC address,
// suitable for passing to call(). registerChan will yield all
// existing registered workers (if any) and new ones as they register.
//
type DoTaskResult struct {
	Ok      bool
	TaskIdx int
	Worker  string
}

func schedule(jobName string, mapFiles []string, nReduce int, phase jobPhase, registerChan chan string) {
	var ntasks int
	var n_other int // number of inputs (for reduce) or outputs (for map)
	switch phase {
	case mapPhase:
		ntasks = len(mapFiles)
		n_other = nReduce
	case reducePhase:
		ntasks = nReduce
		n_other = len(mapFiles)
	}

	fmt.Printf("Schedule: %v %v tasks (%d I/Os)\n", ntasks, phase, n_other)

	// All ntasks tasks have to be scheduled on workers. Once all tasks
	// have completed successfully, schedule() should return.
	//
	// Your code here (Part III, Part IV).
	//
	doneN := 0
	doneChan := make(chan DoTaskResult)
	taskQu := make([]int, ntasks)
	quHead := 0
	for i := 0; i < ntasks; i++ {
		taskQu[i] = i
	}
	for quHead != len(taskQu) {
		taskIdx := taskQu[quHead]
		quHead++
		worker := ""
	FindWorker:
		for {
			select {
			case worker = <-registerChan:
				break FindWorker
			case result := <-doneChan:
				if result.Ok {
					doneN++
					worker = result.Worker
					break FindWorker
				} else {
					taskQu = append(taskQu, result.TaskIdx)
				}
			}
		}
		if len(worker) == 0 {
			panic("Should find worker here!\n")
		}

		arg := DoTaskArgs{
			JobName:       jobName,
			File:          "",
			Phase:         phase,
			TaskNumber:    taskIdx,
			NumOtherPhase: n_other,
		}
		if phase == mapPhase {
			arg.File = mapFiles[taskIdx]
		}

		go func(ttt int) {
			ok := call(worker, "Worker.DoTask", arg, nil)
			if !ok {
				debug("Schedule: assign %v task %v to worker %v fail\n", arg.Phase, arg.TaskNumber, worker)
				doneChan <- DoTaskResult{Ok: false, TaskIdx: taskIdx}
			} else {
				doneChan <- DoTaskResult{Ok: true, Worker: worker}
			}
		}(taskIdx)
	}
	for i := doneN; i < ntasks; i++ {
		<-doneChan
	}
	fmt.Printf("Schedule: %v done\n", phase)
}
