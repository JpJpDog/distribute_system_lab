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
	doneChan := make(chan DoTaskResult)
	doneMap := make(map[int]struct{})
	doneN := 0
	taskQu := make([]int, ntasks)
	taskHead := 0
	for i := 0; i < ntasks; i++ {
		taskQu[i] = i
	}
	idleWorkers := make([]string, 0)
	idleHead := 0
DoneAll:
	for {
		var worker string
		var taskIdx int
		for {
			worker = ""
			for worker == "" {
				select {
				case worker = <-registerChan:
				case result := <-doneChan:
					if result.Ok {
						_, already := doneMap[result.TaskIdx]
						if !already {
							doneN++
							doneMap[result.TaskIdx] = struct{}{}
							if doneN == ntasks {
								break DoneAll
							}
						}
						worker = result.Worker
					} else {
						taskQu = append(taskQu, result.TaskIdx)
						if len(idleWorkers) != idleHead {
							worker = idleWorkers[idleHead]
							idleHead++
						}
					}
				}
			}
			if len(taskQu) != taskHead {
				taskIdx = taskQu[taskHead]
				taskHead++
				break
			}
			idleWorkers = append(idleWorkers, worker)
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

		go func() {
			ok := call(worker, "Worker.DoTask", arg, nil)
			if !ok {
				fmt.Printf("Schedule: assign %v task %v to worker %v fail\n", arg.Phase, arg.TaskNumber, worker)
				doneChan <- DoTaskResult{Ok: false, TaskIdx: taskIdx, Worker: worker}
			} else {
				doneChan <- DoTaskResult{Ok: true, TaskIdx: taskIdx, Worker: worker}
			}
		}()
	}
	fmt.Printf("Schedule: %v done\n", phase)
}
