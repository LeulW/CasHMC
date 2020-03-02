/*********************************************************************************
*  CasHMC v1.3 - 2017.07.10
*  A Cycle-accurate Simulator for Hybrid Memory Cube
*
*  Copyright 2016, Dong-Ik Jeon
*                  Ki-Seok Chung
*                  Hanyang University
*                  estwings57 [at] gmail [dot] com
*
*  Copyright 2020, Leul W. Belayneh
*                  University of Michigan
*                  leulb [at] umich [dot] edu
*  All rights reserved.
*********************************************************************************/

#ifndef LOGICLAYER_H
#define LOGICLAYER_H

//LogicLayer.h

#include <vector>		//vector

#include "DualVectorObject.h"
#include "ConfigValue.h"
#include "CallBack.h"
#include "LinkMaster.h"

using namespace std;

namespace CasHMC
{
	
class LogicLayer : public DualVectorObject<Transaction, Packet>
{
public:
	//
	//Functions
	//
	LogicLayer(ofstream &debugOut_, ofstream &stateOut_, unsigned id);
	virtual ~LogicLayer();
	void RegisterCallbacks(TransCompCB *readCB, TransCompCB *writeCB);
	bool CanAcceptTran();
	//void CallbackReceiveDown(Packet *downEle, bool chkReceive);
	void CallbackReceiveDown(Transaction *downEle, bool chkReceive);
	void CallbackReceiveUp(Packet *upEle, bool chkReceive);
	void Update();
	Packet *ConvTranIntoPacket(Transaction *tran);
	void PrintState();

	//
	//Fields
	//
	unsigned logicContID;
	//vector<DualVectorObject<Packet, Packet> *> downBufferDest;
	DualVectorObject<Packet, Packet> *downBufferDest;
	//vector<LinkMaster *> upBufferDest;
	int inServiceLink;
	vector<unsigned> pendingSegTag;		//Store segment packet tag for returning
	vector<Packet *> pendingSegPacket;	//Store segment packets
	uint64_t requestAccLNG;
	uint64_t responseAccLNG;
	unsigned returnTransCnt;	
	
	TransCompCB *readDone;
	TransCompCB *writeDone;
};

}

#endif
