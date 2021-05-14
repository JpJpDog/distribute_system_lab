package raft

//
// this is an outline of the API that raft must expose to
// the service (or tester). see comments below for
// each of these functions for more details.
//
// rf = Make(...)
//   create a new Raft server.
// rf.Start(command interface{}) (index, term, isleader)
//   start agreement on a new log entry
// rf.GetState() (term, isLeader)
//   ask a Raft for its current term, and whether it thinks it is leader
// ApplyMsg
//   each time a new entry is committed to the log, each Raft peer
//   should send an ApplyMsg to the service (or tester)
//   in the same server.
//

import (
	"math/rand"
	"sort"
	"sync"
	"time"

	"../labrpc"
)

var ElectionTimeout int = 1000
var HeartBeatTime time.Duration = 100

func getElectionTimeout() time.Duration {
	tt := (rand.Intn(ElectionTimeout) + ElectionTimeout) / 2
	return time.Duration(tt)
}

//
// as each Raft peer becomes aware that successive log entries are
// committed, the peer should send an ApplyMsg to the service (or
// tester) on the same server, via the applyCh passed to Make(). set
// CommandValid to true to indicate that the ApplyMsg contains a newly
// committed log entry.
//
// in Lab 3 you'll want to send other kinds of messages (e.g.,
// snapshots) on the applyCh; at that point you can add fields to
// ApplyMsg, but set CommandValid to false for these other uses.
//
type ApplyMsg struct {
	CommandValid bool
	Command      interface{}
	CommandIndex int
}

type LogEntry struct {
	Content interface{}
	TermIdx int
}

//
// A Go object implementing a single Raft peer.
//
type Raft struct {
	mu        sync.Mutex          // Lock to protect shared access to this peer's state
	peers     []*labrpc.ClientEnd // RPC end points of all peers
	persister *Persister          // Object to hold this peer's persisted state
	me        int                 // this peer's index into peers[]

	// Your data here (2A, 2B, 2C).
	// Look at the paper's Figure 2 for a description of what
	// state a Raft server must maintain.

	// this will be read and written by RPC so should protected by mutex
	currentTerm int
	votedFor    int
	status      int //0:leader,1:candidate,2:follower

	log []LogEntry

	authCome   chan struct{}
	toFollower chan struct{}

	//volatile state
	commitIndex int
	lastApplied int

	//volatile leader state
	nextIndex  []int
	matchIndex []int

	applyCh chan ApplyMsg
}

// return currentTerm and whether this server
// believes it is the leader.
func (rf *Raft) GetState() (int, bool) {
	var term int
	var isleader bool
	// Your code here (2A).
	rf.mu.Lock()
	term = rf.currentTerm
	isleader = rf.status == 0
	rf.mu.Unlock()
	return term, isleader
}

//
// save Raft's persistent state to stable storage,
// where it can later be retrieved after a crash and restart.
// see paper's Figure 2 for a description of what should be persistent.
//
func (rf *Raft) persist() {
	// Your code here (2C).
	// Example:
	// w := new(bytes.Buffer)
	// e := labgob.NewEncoder(w)
	// e.Encode(rf.xxx)
	// e.Encode(rf.yyy)
	// data := w.Bytes()
	// rf.persister.SaveRaftState(data)
}

//
// restore previously persisted state.
//
func (rf *Raft) readPersist(data []byte) {
	if data == nil || len(data) < 1 { // bootstrap without any state?
		return
	}
	// Your code here (2C).
	// Example:
	// r := bytes.NewBuffer(data)
	// d := labgob.NewDecoder(r)
	// var xxx
	// var yyy
	// if d.Decode(&xxx) != nil ||
	//    d.Decode(&yyy) != nil {
	//   error...
	// } else {
	//   rf.xxx = xxx
	//   rf.yyy = yyy
	// }
}

type RequestVoteArgs struct {
	// Your data here (2A, 2B).
	Term         int
	CandidateId  int
	LastLogIndex int
	LastLogTerm  int
}

type RequestVoteReply struct {
	// Your data here (2A).
	Term        int
	VoteGranted bool
}

func (rf *Raft) logNewer(args *RequestVoteArgs) bool {
	latest := rf.log[len(rf.log)-1]
	if args.LastLogTerm > latest.TermIdx {
		return true
	} else if args.LastLogTerm == latest.TermIdx {
		return args.LastLogIndex+1 >= len(rf.log)
	}
	return false
}

func (rf *Raft) updateTerm(term int) bool {
	if term > rf.currentTerm {
		DPrintf("@@@ %v update to term %v and be follower\n", rf.me, term)
		if rf.status != 2 && len(rf.toFollower) == 0 {
			rf.toFollower <- struct{}{}
		}
		rf.currentTerm = term
		rf.votedFor = -1
		rf.status = 2
		return true
	}
	return false
}

func (rf *Raft) RequestVote(args *RequestVoteArgs, reply *RequestVoteReply) {
	// Your code here (2A, 2B).
	rf.mu.Lock()
	DPrintf("@@@ %v recv RequestVote from %v\n", rf.me, args.CandidateId)
	reply.Term = rf.currentTerm
	rf.updateTerm(args.Term)
	if args.Term < rf.currentTerm {
		DPrintf("@@@ %v refuse %v for old term\n", rf.me, args.CandidateId)
		reply.VoteGranted = false
	} else if rf.votedFor != -1 && rf.votedFor != args.CandidateId {
		DPrintf("@@@ %v refuse %v for already vote\n", rf.me, args.CandidateId)
		reply.VoteGranted = false
	} else if !rf.logNewer(args) {
		DPrintf("@@@ %v refuse %v for old log\n", rf.me, args.CandidateId)
		reply.VoteGranted = false
	} else {
		DPrintf("@@@ %v agree vote %v in term %v\n", rf.me, args.CandidateId, rf.currentTerm)
		rf.votedFor = args.CandidateId
		reply.VoteGranted = true
	}
	rf.mu.Unlock()
}

func (rf *Raft) sendRequestVote(server int, args *RequestVoteArgs, reply *RequestVoteReply) bool {
	return rf.peers[server].Call("Raft.RequestVote", args, reply)
}

type AppendEntriesArgs struct {
	Term         int
	LeaderId     int
	PrevLogIndex int
	PrevLogTerm  int
	Entries      []LogEntry
	LeaderCommit int
}

type AppendEntriesReply struct {
	Term    int
	Success bool
}

func (rf *Raft) AppendEntries(args *AppendEntriesArgs, reply *AppendEntriesReply) {
	rf.mu.Lock()
	reply.Term = rf.currentTerm
	DPrintf("@@@ %v recv AppendEntries\n", rf.me)
	rf.updateTerm(args.Term)
	if rf.currentTerm > args.Term {
		DPrintf("@@@ %v not success because old term\n", rf.me)
		reply.Success = false
		rf.mu.Unlock()
		return
	}
	if len(rf.log) <= args.PrevLogIndex || rf.log[args.PrevLogIndex].TermIdx != args.PrevLogTerm { //not contain PrevIndex entry with PrevTerm
		DPrintf("@@@ %v not success because not has prev log\n", rf.me)
		reply.Success = false
	} else {
		DPrintf("@@@ %v success\n", rf.me)
		reply.Success = true
		newIdx := 0
		oldIdx := args.PrevLogIndex + 1
		for newIdx < len(args.Entries) && oldIdx < len(rf.log) && args.Entries[newIdx].TermIdx == rf.log[oldIdx].TermIdx {
			newIdx++
			oldIdx++
		}
		rf.log = rf.log[0:oldIdx]
		for newIdx < len(args.Entries) {
			rf.log = append(rf.log, args.Entries[newIdx])
			DPrintf("@@@ %v append index %v value %v\n", rf.me, len(rf.log)-1, args.Entries[newIdx])
			newIdx++
		}
		rf.lastApplied = len(rf.log) - 1
		if args.LeaderCommit > rf.commitIndex {
			newCommit := MinOf(len(rf.log)-1, args.LeaderCommit)
			appMsgs := make([]ApplyMsg, 0)
			for idx := rf.commitIndex + 1; idx <= newCommit; idx++ {
				appMsg := ApplyMsg{
					CommandValid: true,
					Command:      rf.log[idx].Content,
					CommandIndex: idx,
				}
				appMsgs = append(appMsgs, appMsg)
				DPrintf("@@@ %v commit index: %v value: %v", rf.me, idx, appMsg.Command)
			}
			rf.commitIndex = newCommit
			go func() {
				for _, appMsg := range appMsgs {
					rf.applyCh <- appMsg
				}
			}()
		}
	}
	if len(rf.authCome) == 0 { // prevent blocking
		rf.authCome <- struct{}{}
		DPrintf("@@@ follower %v find leader auth\n", rf.me)
	}
	rf.mu.Unlock()
}

func (rf *Raft) sendAppendEntries(server int, args *AppendEntriesArgs, reply *AppendEntriesReply) bool {
	return rf.peers[server].Call("Raft.AppendEntries", args, reply)
}

//
// the service using Raft (e.g. a k/v server) wants to start
// agreement on the next command to be appended to Raft's log. if this
// server isn't the leader, returns false. otherwise start the
// agreement and return immediately. there is no guarantee that this
// command will ever be committed to the Raft log, since the leader
// may fail or lose an election. even if the Raft instance has been killed,
// this function should return gracefully.
//
// the first return value is the index that the command will appear at
// if it's ever committed. the second return value is the current
// term. the third return value is true if this server believes it is
// the leader.
//
func (rf *Raft) Start(command interface{}) (int, int, bool) {
	index := -1
	term := -1
	isLeader := true

	// Your code here (2B).
	rf.mu.Lock()
	index = len(rf.log)
	term = rf.currentTerm
	isLeader = rf.status == 0
	if isLeader {
		DPrintf("### leader %v append %v, term %v\n", rf.me, command, rf.currentTerm)
		rf.log = append(rf.log, LogEntry{
			Content: command,
			TermIdx: rf.currentTerm,
		})
		rf.lastApplied = len(rf.log)
		rf.matchIndex[rf.me]++
	}
	rf.mu.Unlock()
	return index, term, isLeader
}

//
// the tester calls Kill() when a Raft instance won't
// be needed again. you are not required to do anything
// in Kill(), but it might be convenient to (for example)
// turn off debug output from this instance.
//
func (rf *Raft) Kill() {
	// Your code here, if desired.
	DPrintf("!!! kill %v\n", rf.me)
}

func (rf *Raft) leaderRoutine() {
	for i := range rf.peers {
		rf.nextIndex[i] = len(rf.log)
		rf.matchIndex[i] = 0
	}
	rf.matchIndex[rf.me] = rf.lastApplied
	rf.nextIndex[rf.me] = rf.lastApplied + 1
	rf.mu.Unlock()
	replies := make([]AppendEntriesReply, len(rf.peers))

	timeoutChan := make(chan struct{}) //set ticker
	timerClose := make(chan struct{})
	go func() { // start a heartbeat ticker
		for {
			time.Sleep(time.Millisecond * HeartBeatTime)
			select {
			case <-timerClose: //end the ticker goroutine
				return
			default:
				timeoutChan <- struct{}{}
			}
		}
	}()
	for {
		appendChan := make(chan int) // make a new channel everytime
		rf.mu.Lock()
		args := make([]AppendEntriesArgs, len(rf.peers))
		for i := range rf.peers {
			args[i] = AppendEntriesArgs{ // invariable parameter in func
				Term:     rf.currentTerm,
				LeaderId: rf.me,
			}

		}
		for peer := range rf.peers {
			if peer == rf.me {
				continue
			}
			nextIdx := rf.nextIndex[peer]
			args[peer].PrevLogIndex = nextIdx - 1
			args[peer].PrevLogTerm = rf.log[nextIdx-1].TermIdx
			args[peer].Entries = rf.log[nextIdx:]
			args[peer].LeaderCommit = rf.commitIndex
			go func(idx int) {
				DPrintf("### leader %v send AppendEntries to follower %v\n", rf.me, idx)
				if rf.sendAppendEntries(idx, &args[idx], &replies[idx]) {
					appendChan <- idx
				}
			}(peer)
		}
		rf.mu.Unlock()
	WaitAppendReply:
		for {
			select {
			case peer := <-appendChan:
				reply := replies[peer]
				rf.mu.Lock()
				if reply.Term > rf.currentTerm { // leader to follower
					DPrintf("### leader %v be follower because append reply\n", rf.me)
					rf.currentTerm = reply.Term
					rf.votedFor = -1
					rf.status = 2
					timerClose <- struct{}{}
					return
				}
				if reply.Success {
					DPrintf("### leader %v recv AppendEntriesReply from %v success\n", rf.me, peer)
					if len(args[peer].Entries) != 0 {
						rf.matchIndex[peer] = args[peer].PrevLogIndex + len(args[peer].Entries)
						rf.nextIndex[peer] = rf.matchIndex[peer] + 1
						sortMatchIdx := make([]int, len(rf.peers))
						copy(sortMatchIdx, rf.matchIndex)
						sort.Ints(sortMatchIdx)
						maxMajorityIdx := sortMatchIdx[len(rf.peers)/2]
						if rf.commitIndex < maxMajorityIdx {
							appMsgs := make([]ApplyMsg, 0)
							for idx := rf.commitIndex + 1; idx <= maxMajorityIdx; idx++ {
								appMsg := ApplyMsg{
									CommandValid: true,
									Command:      rf.log[idx].Content,
									CommandIndex: idx,
								}
								DPrintf("### leader %v commit index: %v value %v\n", rf.me, idx, appMsg.Command)
								appMsgs = append(appMsgs, appMsg)
							}
							go func() {
								for _, appMsg := range appMsgs {
									rf.applyCh <- appMsg
								}
							}()
						}
						rf.commitIndex = maxMajorityIdx
					}
				} else {
					DPrintf("### leader %v recv AppendEntriesReply from %v fail\n", rf.me, peer)
					if rf.nextIndex[peer] > 1 {
						rf.nextIndex[peer]--
					}
				}
				rf.mu.Unlock()
			case <-timeoutChan:
				break WaitAppendReply
			case <-rf.toFollower:
				timerClose <- struct{}{}
				rf.mu.Lock()
				DPrintf("### leader %v be follower\n", rf.me)
				return
			}
		}
	}
}

func (rf *Raft) candidateRoutine() {
	for {
		rf.currentTerm++
		rf.votedFor = rf.me
		args := RequestVoteArgs{
			CandidateId:  rf.me,
			Term:         rf.currentTerm,
			LastLogIndex: len(rf.log) - 1,
			LastLogTerm:  rf.log[len(rf.log)-1].TermIdx,
		}
		replies := make([]RequestVoteReply, len(rf.peers))
		rf.mu.Unlock()
		voteChan := make(chan int)
		timeoutChan := make(chan struct{})
		go func() {
			time.Sleep(time.Millisecond * getElectionTimeout())
			timeoutChan <- struct{}{}
		}()
		for peer := range rf.peers {
			if peer == rf.me {
				continue
			}
			go func(peer int) {
				DPrintf("### candidate %v send vote request to %v\n", rf.me, peer)
				if rf.sendRequestVote(peer, &args, &replies[peer]) {
					voteChan <- peer
				}
			}(peer)
		}
		grantN := 1 //one for itself
	WaitReply:
		for {
			select {
			case <-timeoutChan:
				DPrintf("### candidate %v timeout, again\n", rf.me)
				break WaitReply
			case peer := <-voteChan:
				reply := replies[peer]
				rf.mu.Lock()
				if rf.currentTerm < reply.Term {
					DPrintf("### candidate %v be follower because vote reply\n", rf.me)
					rf.currentTerm = reply.Term
					rf.votedFor = -1
					rf.status = 2
					return
				}
				rf.mu.Unlock()
				if reply.VoteGranted {
					grantN++
					DPrintf("### candidate %v recv vote, cur %v vote\n", rf.me, grantN)
					if grantN > len(rf.peers)/2 {
						DPrintf("### candidate %v be leader!\n", rf.me)
						rf.mu.Lock()
						rf.status = 0
						return
					}
				}
			case <-rf.toFollower:
				rf.mu.Lock()
				DPrintf("### candidate %v be follower\n", rf.me)
				return
			}
		}
		rf.mu.Lock()
	}
}

func (rf *Raft) followerRoutine() {
	rf.mu.Unlock()
	currentTimerId := 0
	timeoutChan := make(chan int)
	for {
		currentTimerId++
		go func(timerId int) {
			time.Sleep(time.Millisecond * getElectionTimeout())
			timeoutChan <- timerId
		}(currentTimerId)
	WaitHeartbeat:
		for {
			select {
			case timerId := <-timeoutChan:
				if timerId == currentTimerId {
					rf.mu.Lock()
					DPrintf("### follower %v timeout, become candidate\n", rf.me)
					rf.status = 1
					return
				}
			case <-rf.authCome:
				DPrintf("### follower %v get auth, keep follower\n", rf.me)
				break WaitHeartbeat
			}
		}
	}
}

//
// the service or tester wants to create a Raft server. the ports
// of all the Raft servers (including this one) are in peers[]. this
// server's port is peers[me]. all the servers' peers[] arrays
// have the same order. persister is a place for this server to
// save its persistent state, and also initially holds the most
// recent saved state, if any. applyCh is a channel on which the
// tester or service expects Raft to send ApplyMsg messages.
// Make() must return quickly, so it should start goroutines
// for any long-running work.
//
func Make(peers []*labrpc.ClientEnd, me int,
	persister *Persister, applyCh chan ApplyMsg) *Raft {
	rf := &Raft{}
	rf.peers = peers
	rf.persister = persister
	rf.me = me
	//should be init before the first RPC call
	rf.currentTerm = 0
	rf.votedFor = -1
	rf.status = 2
	rf.authCome = make(chan struct{}, 1)
	rf.toFollower = make(chan struct{}, 1)

	rf.log = make([]LogEntry, 1)
	rf.log[0].TermIdx = 0
	rf.commitIndex = 0
	rf.lastApplied = 0
	rf.nextIndex = make([]int, len(rf.peers))
	rf.matchIndex = make([]int, len(rf.peers))

	rf.applyCh = applyCh

	// Your initialization code here (2A, 2B, 2C).

	DPrintf("node %v start\n", rf.me)
	go func() {
		rf.mu.Lock()
		for {
			switch rf.status {
			case 0:
				rf.leaderRoutine()
			case 1:
				rf.candidateRoutine()
			case 2:
				rf.followerRoutine()
			}
		}
	}()

	// initialize from state persisted before a crash
	rf.readPersist(persister.ReadRaftState())

	return rf
}
