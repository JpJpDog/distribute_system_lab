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
	"sync"
	"time"

	"../labrpc"
)

// import "bytes"
// import "labgob"

var ElectionTimeout int = 500
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

	authCome   chan struct{}
	toFollower chan struct{}
}

// return currentTerm and whether this server
// believes it is the leader.
func (rf *Raft) GetState() (int, bool) {
	var term int
	var isleader bool
	// Your code here (2A).
	rf.mu.Lock()
	defer rf.mu.Unlock()
	term = rf.currentTerm
	isleader = rf.status == 0
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
	Term        int
	CandidateId int
}

type RequestVoteReply struct {
	// Your data here (2A).
	Term        int
	VoteGranted bool
}

func (rf *Raft) RequestVote(args *RequestVoteArgs, reply *RequestVoteReply) {
	// Your code here (2A, 2B).
	rf.mu.Lock()
	DPrintf("### %v RPC recv vote request from %v. ", rf.me, args.CandidateId)
	reply.Term = rf.currentTerm
	if rf.currentTerm < args.Term {
		if rf.status != 2 && len(rf.toFollower) == 0 {
			rf.toFollower <- struct{}{}
		}
		rf.currentTerm = args.Term
		rf.votedFor = args.CandidateId
		rf.status = 2
		reply.VoteGranted = true
	} else if rf.currentTerm > args.Term {
		reply.VoteGranted = false
	} else if rf.votedFor == -1 || rf.votedFor == args.CandidateId {
		reply.VoteGranted = true
	} else {
		reply.VoteGranted = false
	}
	if reply.VoteGranted {
		DPrintf("vote!\n")
	} else {
		DPrintf("not vote!\n")
	}
	rf.mu.Unlock()
}

func (rf *Raft) sendRequestVote(server int, args *RequestVoteArgs, reply *RequestVoteReply) bool {
	return rf.peers[server].Call("Raft.RequestVote", args, reply)
}

type AppendEntriesArgs struct {
	Term     int
	LeaderId int
}

type AppendEntriesReply struct {
	Term int
}

func (rf *Raft) AppendEntries(args *AppendEntriesArgs, reply *AppendEntriesReply) {
	rf.mu.Lock()
	DPrintf("### %v RPC recv auth from %v\n", rf.me, args.LeaderId)
	reply.Term = rf.currentTerm
	if args.Term > rf.currentTerm {
		if rf.status != 2 && len(rf.toFollower) == 0 {
			rf.toFollower <- struct{}{}
		}
		rf.currentTerm = args.Term
		rf.votedFor = -1
		rf.status = 2
	}
	rf.mu.Unlock()
	if len(rf.authCome) == 0 { // may race but not care
		rf.authCome <- struct{}{}
	}
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
}

func (rf *Raft) raftRoutine() {
	rf.mu.Lock()
	for {
		switch rf.status {
		case 0:
			args := AppendEntriesArgs{
				Term:     rf.currentTerm,
				LeaderId: rf.me,
			}
			rf.mu.Unlock()
			replies := make([]AppendEntriesReply, len(rf.peers))
			timeoutChan := make(chan struct{})
			timerClose := make(chan struct{})
			go func() { // start a heartbeat ticker
				DPrintf("### leader %v send append\n", rf.me)
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
		Leader:
			for {
				appendChan := make(chan int)
				for peer := range rf.peers {
					if peer != rf.me {
						go func(peer int) {
							if rf.sendAppendEntries(peer, &args, &replies[peer]) {
								appendChan <- peer
							}
						}(peer)
					}
				}
			WaitAppendReply:
				for {
					select {
					case peer := <-appendChan:
						reply := replies[peer]
						rf.mu.Lock()
						DPrintf("### leader %v recv append reply\n", rf.me)
						if reply.Term > rf.currentTerm {
							rf.currentTerm = reply.Term
							rf.votedFor = -1
							rf.status = 2
							timerClose <- struct{}{}
							break Leader
						}
						rf.mu.Unlock()
					case <-timeoutChan:
						break WaitAppendReply
					case <-rf.toFollower:
						timerClose <- struct{}{}
						rf.mu.Lock()
						DPrintf("### leader %v be follower\n", rf.me)
						if len(rf.toFollower) != 0 {
							<-rf.toFollower
						}
						break Leader
					}
				}
			}
		case 1:
		Candidate:
			for {
				rf.currentTerm++
				rf.votedFor = rf.me
				args := RequestVoteArgs{
					Term:        rf.currentTerm,
					CandidateId: rf.me,
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
					if peer != rf.me {
						go func(peer int) {
							if rf.sendRequestVote(peer, &args, &replies[peer]) {
								voteChan <- peer
							}
						}(peer)
					}
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
						DPrintf("### candidate %v recv vote, cur %v vote\n", rf.me, grantN)
						if rf.currentTerm < reply.Term {
							rf.currentTerm = reply.Term
							rf.votedFor = -1
							rf.status = 2
							DPrintf("### candidate %v be leader!\n", rf.me)
							break Candidate
						}
						rf.mu.Unlock()
						if reply.VoteGranted {
							grantN++
							if grantN > len(rf.peers)/2 {
								rf.mu.Lock()
								rf.status = 0
								break Candidate
							}
						}
					case <-rf.toFollower:
						rf.mu.Lock()
						DPrintf("### candidate %v be follower\n", rf.me)
						if len(rf.toFollower) != 0 {
							<-rf.toFollower
						}
						break Candidate
					}
				}
				rf.mu.Lock()
			}
		case 2:
			rf.mu.Unlock()
			currentTimerId := 0
			timeoutChan := make(chan int)
		Follower:
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
							DPrintf("### follower %v timeout, because candidate\n", rf.me)
							rf.mu.Lock()
							rf.status = 1
							break Follower
						}
					case <-rf.authCome:
						DPrintf("### follower %v get auth, keep\n", rf.me)
						break WaitHeartbeat
					}
				}
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
	rf.authCome = make(chan struct{})
	rf.toFollower = make(chan struct{})

	// Your initialization code here (2A, 2B, 2C).

	DPrintf("node %v start\n", rf.me)
	go rf.raftRoutine()

	// initialize from state persisted before a crash
	rf.readPersist(persister.ReadRaftState())

	return rf
}
