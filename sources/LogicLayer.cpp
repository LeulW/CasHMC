/*********************************************************************************
*  CasHMC v1.3 - 2017.07.10
*  A Cycle-accurate Simulator for Hybrid Memory Cube
*
*  Copyright 2016, Dong-Ik Jeon
*                  Ki-Seok Chung
*                  Hanyang University
*                  estwings57 [at] gmail [dot] com
*  All rights reserved.
*********************************************************************************/

#include "LogicLayer.h"

namespace CasHMC
{
	
LogicLayer::LogicLayer(ofstream &debugOut_, ofstream &stateOut_, unsigned id):
	DualVectorObject<Transaction, Packet>(debugOut_, stateOut_, MAX_VLT_BUF, MAX_VLT_BUF),
	logicContID(id),
	readDone(NULL),
	writeDone(NULL)
{
	classID << logicContID;
	header = "        (LL_" + classID.str() + ")";
	
	inServiceLink = -1;

	//downBufferDest = vector<DualVectorObject<Packet, Packet> *>(NUM_VAULTS, NULL);
	//upBufferDest = vector<LinkMaster *>(NUM_LINKS, NULL);
}

LogicLayer::~LogicLayer()
{	
	//downBufferDest.clear();
	//upBufferDest.clear();
	pendingSegTag.clear(); 
	pendingSegPacket.clear(); 
}

//
//Register read and write callback funtions
//
void LogicLayer::RegisterCallbacks(TransCompCB *readCB, TransCompCB *writeCB)
{
	readDone = readCB;
	writeDone = writeCB;
}

//
//Callback Adding packet
//
void LogicLayer::CallbackReceiveDown(Transaction *downEle, bool chkReceive)
{
    //cout<<"Packet in the downlink of the Logic Layer!"<<endl;
	downEle->address &= ((uint64_t)NUM_VAULTS*NUM_BANKS*NUM_COLS*NUM_ROWS - 1);
	downEle->trace->tranTransmitTime = currentClockCycle;
	if(downEle->transactionType == DATA_WRITE)	downEle->trace->statis->hmcTransmitSize += downEle->dataSize;
/*	if(chkReceive) {
		DEBUG(ALI(18)<<header<<ALI(15)<<*downEle<<"Down) RECEIVING packet");
	}
	else {
		DEBUG(ALI(18)<<header<<ALI(15)<<*downEle<<"Down) packet buffer FULL");
	}*/
}
void LogicLayer::CallbackReceiveUp(Packet *upEle, bool chkReceive)
{
    //cout<<"Packet in the uplink of the Logic Layer!"<<endl;
/*	if(chkReceive) {
		DEBUG(ALI(18)<<header<<ALI(15)<<*upEle<<"Up)   RECEIVING packet");
	}
	else {
		DEBUG(ALI(18)<<header<<ALI(15)<<*upEle<<"Up)   packet buffer FULL");
	}*/
}

bool LogicLayer::CanAcceptTran(void)
{
	if(downBuffers.size() + 1 <= downBufferMax) {
		return true;
	}
	else {
		return false;
	}
}

//
//Update the state of crossbar switch
//
void LogicLayer::Update()
{	
	//Downstream buffer state
	if(bufPopDelay==0 && downBuffers.size() > 0) {
		//Check packet dependency
		bool dependent = false;
		for(int i=0; i<downBufferDest->downBuffers.size(); i++) {
			if(downBufferDest->downBuffers[i] != NULL) {
				unsigned maxBlockBit = _log2(ADDRESS_MAPPING);
				if((downBufferDest->downBuffers[i]->ADRS >> maxBlockBit) == (downBuffers[0]->address >> maxBlockBit)) {
					dependent = true;
					DEBUG(ALI(18)<<header<<ALI(15)<<*downBuffers[0]<<"Down) This transaction has a DEPENDENCY with "<<*downBufferDest->downBuffers[i]);
					break;
				}
			}
		}
		if(!dependent){
			//Check request size and the maximum block size
			/*if(downBuffers[0]->reqDataSize > ADDRESS_MAPPING) {
				int segPacket = ceil((double)downBuffers[0]->reqDataSize/ADDRESS_MAPPING);
				downBuffers[0]->reqDataSize = ADDRESS_MAPPING;
				DEBUG(ALI(18)<<header<<ALI(15)<<*downBuffers[0]<<"Down) Packet is DIVIDED into "<<segPacket<<" segment packets by max block size");
					
				//the packet is divided into segment packets.
				Packet *tempPacket = downBuffers[0];
				downBuffers.erase(downBuffers.begin(), downBuffers.begin()+downBuffers[0]->LNG);
				if(tempPacket->LNG > 1)	tempPacket->LNG = 1 + ADDRESS_MAPPING/16;	//one flit is 16 bytes
				for(int j=0; j<segPacket; j++) {
					Packet *vaultPacket = new Packet(*tempPacket);
					vaultPacket->ADRS += j*ADDRESS_MAPPING;
					if(j>0)	vaultPacket->trace = NULL;
					downBuffers.insert(downBuffers.begin()+i, vaultPacket);
					for(int k=1; k<vaultPacket->LNG; k++) {		//Virtual tail packet
						downBuffers.insert(downBuffers.begin()+1, NULL);
					}
					i += vaultPacket->LNG;
					vaultPacket->segment = true;
					pendingSegTag.push_back(vaultPacket->TAG);
				}
				delete tempPacket;
			}
			else{*/
				Packet *packet = ConvTranIntoPacket(downBuffers[0]);
				unsigned vaultMap = (packet->ADRS >> (33-_log2(NUM_VAULTS/*ADDRESS_MAPPING*/))) & (NUM_VAULTS-1);
				if(downBufferDest->ReceiveDown(packet)) {
					DEBUG(ALI(18)<<header<<ALI(15)<<*downBuffers[0]<<"Down) SENDING packet to vault controller "<<vaultMap<<" (VC_"<<vaultMap<<")");		
					//cout<<"Down SENDING packet to vault controller "<<vaultMap<<" (VC_"<<vaultMap<<")"<<packet->ADRS<<endl;;
					// cout<<"Down SENDING packet to vault controller "<<vaultMap<<" (VC_"<<vaultMap<<")"<<endl;
					if(!((packet->CMD >= P_WR16 && packet->CMD <= P_WR128)
					|| packet->CMD == P_WR256 || packet->CMD == P_2ADD8
					|| packet->CMD == P_ADD16 || packet->CMD == P_INC8
					|| packet->CMD == P_BWR)) {
						returnTransCnt++;
					}
					//Call callback function if posted write packet is tranmitted
					if((packet->CMD >= P_WR16 && packet->CMD <= P_WR128) || packet->CMD == P_WR256) {
						if(writeDone != NULL) {
							(*writeDone)(packet->ADRS, currentClockCycle);
						}
					}
					requestAccLNG += packet->LNG;
					delete downBuffers[0];
					downBuffers.erase(downBuffers.begin());
				}
				else {
					packet->ReductGlobalTAG();
					delete packet;
					//DEBUG(ALI(18)<<header<<ALI(15)<<*downBuffers[0]<<"Down) Vault controller buffer FULL");	
				}
			//}
		}
		else{
			//Check request size and the maximum block size
			/*if(downBuffers[0]->reqDataSize > ADDRESS_MAPPING) {
				int segPacket = ceil((double)downBuffers[0]->reqDataSize/ADDRESS_MAPPING);
				downBuffers[0]->reqDataSize = ADDRESS_MAPPING;
				DEBUG(ALI(18)<<header<<ALI(15)<<*downBuffers[0]<<"Down) Packet is DIVIDED into "<<segPacket<<" segment packets by max block size");
					
				//the packet is divided into segment packets.
				Packet *tempPacket = downBuffers[0];
				downBuffers.erase(downBuffers.begin(), downBuffers.begin()+downBuffers[0]->LNG);
				if(tempPacket->LNG > 1)	tempPacket->LNG = 1 + ADDRESS_MAPPING/16;	//one flit is 16 bytes
				for(int j=0; j<segPacket; j++) {
					Packet *vaultPacket = new Packet(*tempPacket);
					vaultPacket->ADRS += j*ADDRESS_MAPPING;
					if(j>0)	vaultPacket->trace = NULL;
					downBuffers.insert(downBuffers.begin()+i, vaultPacket);
					for(int k=1; k<vaultPacket->LNG; k++) {		//Virtual tail packet
						downBuffers.insert(downBuffers.begin()+1, NULL);
					}
					i += vaultPacket->LNG;
					vaultPacket->segment = true;
					pendingSegTag.push_back(vaultPacket->TAG);
				}
				delete tempPacket;
			}
			else{*/
				Packet *packet = ConvTranIntoPacket(downBuffers[0]);
				unsigned vaultMap = (packet->ADRS >> (33-_log2(NUM_VAULTS/*ADDRESS_MAPPING*/))) & (NUM_VAULTS-1);
				if(downBufferDest->ReceiveDown(packet)) {
					DEBUG(ALI(18)<<header<<ALI(15)<<*downBuffers[0]<<"Down) SENDING packet to vault controller "<<vaultMap<<" (VC_"<<vaultMap<<")");		
					//cout<<"Down SENDING packet to vault controller "<<vaultMap<<" (VC_"<<vaultMap<<")"<<packet->ADRS<<endl;;
					delete downBuffers[0];
					downBuffers.erase(downBuffers.begin());
				}
				else {
					packet->ReductGlobalTAG();
					delete packet;
					//DEBUG(ALI(18)<<header<<ALI(15)<<*downBuffers[0]<<"Down) Vault controller buffer FULL");	
				}
			//}
		}
	
	}
	
	//Upstream buffer state
	if(upBuffers.size() > 0) {
		//Make sure that buffer[0] is not virtual tail packet.
		if(upBuffers[0] == NULL) {
			ERROR(header<<"  == Error - Logic Layer up buffer[0] is NULL (It could be one of virtual tail packet occupying packet length  (CurrentClock : "<<currentClockCycle<<")");
			exit(0);
		}
		else {
			DE_CR(ALI(18)<<header<<ALI(15)<<*upBuffers[0]<<"Up)   RETURNING transaction to system bus");
			//cout<<"Up)   RETURNING transaction to system bus"<<" (LL_"<<this->logicContID<<")"<<endl;
			upBuffers[0]->trace->tranFullLat = currentClockCycle - upBuffers[0]->trace->tranTransmitTime;
			if(upBuffers[0]->CMD == RD_RS) {
				upBuffers[0]->trace->statis->hmcTransmitSize += (upBuffers[0]->LNG - 1)*16;
			}
			returnTransCnt--;
			
			//Call callback function if it is registered
			if(upBuffers[0]->CMD == WR_RS) {
				if(writeDone != NULL) {
					(*writeDone)(upBuffers[0]->ADRS, currentClockCycle);
				}
			}
			else if(upBuffers[0]->CMD == RD_RS) {
				if(readDone != NULL) {
					(*readDone)(upBuffers[0]->ADRS, currentClockCycle);
				}
			}
			int packetLNG = upBuffers[0]->LNG;
			responseAccLNG += packetLNG;
			delete upBuffers[0]->trace;
			delete upBuffers[0];
			upBuffers.erase(upBuffers.begin(), upBuffers.begin()+packetLNG);
		}
	}
	
	Step();
}

//
//Convert transaction into packet-based protocol (FLITs) where the packets consist of 128-bit flow units
//
Packet *LogicLayer::ConvTranIntoPacket(Transaction *tran)
{
	unsigned packetLength;
	unsigned reqDataSize=16;
	PacketCommandType cmdtype;
	
	switch(tran->transactionType) {
		case DATA_READ:
			packetLength = 1; //header + tail
			reqDataSize = tran->dataSize;
			switch(tran->dataSize) {
				case 16:	cmdtype = RD16;		break;
				case 32:	cmdtype = RD32;		break;
				case 48:	cmdtype = RD48;		break;
				case 64:	cmdtype = RD64;		break;
				case 80:	cmdtype = RD80;		break;
				case 96:	cmdtype = RD96;		break;
				case 112:	cmdtype = RD112;	break;
				case 128:	cmdtype = RD128;	break;
				case 256:	cmdtype = RD256;	break;
				default:
					ERROR(header<<"  == Error - WRONG transaction data size  (CurrentClock : "<<currentClockCycle<<")");
					exit(0);
			}
			break;
		case DATA_WRITE:		
			packetLength = tran->dataSize /*[byte] Size of data*/ / 16 /*packet 16-byte*/ + 1 /*header + tail*/;
			reqDataSize = tran->dataSize;
			switch(tran->dataSize) {
				case 16:	cmdtype = WR16;		break;
				case 32:	cmdtype = WR32;		break;
				case 48:	cmdtype = WR48;		break;
				case 64:	cmdtype = WR64;		break;
				case 80:	cmdtype = WR80;		break;
				case 96:	cmdtype = WR96;		break;
				case 112:	cmdtype = WR112;	break;
				case 128:	cmdtype = WR128;	break;
				case 256:	cmdtype = WR256;	break;
				default:
					ERROR(header<<"  == Error - WRONG transaction data size  (CurrentClock : "<<currentClockCycle<<")");
					exit(0);
			}
			break;
		//Arithmetic atomic
		case ATM_2ADD8:		cmdtype = _2ADD8;	packetLength = 2;	break;
		case ATM_ADD16:		cmdtype = ADD16;	packetLength = 2;	break;
		case ATM_P_2ADD8:	cmdtype = P_2ADD8;	packetLength = 2;	break;
		case ATM_P_ADD16:	cmdtype = P_ADD16;	packetLength = 2;	break;
		case ATM_2ADDS8R:	cmdtype = _2ADDS8R;	packetLength = 2;	break;
		case ATM_ADDS16R:	cmdtype = ADDS16R;	packetLength = 2;	break;
		case ATM_INC8:		cmdtype = INC8;		packetLength = 1;	break;
		case ATM_P_INC8:	cmdtype = P_INC8;	packetLength = 1;	break;
		//Boolean atomic
		case ATM_XOR16:		cmdtype = XOR16;	packetLength = 2;	break;
		case ATM_OR16:		cmdtype = OR16;		packetLength = 2;	break;
		case ATM_NOR16:		cmdtype = NOR16;	packetLength = 2;	break;
		case ATM_AND16:		cmdtype = AND16;	packetLength = 2;	break;
		case ATM_NAND16:	cmdtype = NAND16;	packetLength = 2;	break;
		//Comparison atomic
		case ATM_CASGT8:	cmdtype = CASGT8;	packetLength = 2;	break;
		case ATM_CASLT8:	cmdtype = CASLT8;	packetLength = 2;	break;
		case ATM_CASGT16:	cmdtype = CASGT16;	packetLength = 2;	break;
		case ATM_CASLT16:	cmdtype = CASLT16;	packetLength = 2;	break;
		case ATM_CASEQ8:	cmdtype = CASEQ8;	packetLength = 2;	break;
		case ATM_CASZERO16:	cmdtype = CASZERO16;packetLength = 2;	break;
		case ATM_EQ16:		cmdtype = EQ16;		packetLength = 2;	break;
		case ATM_EQ8:		cmdtype = EQ8;		packetLength = 2;	break;
		//Bitwise atomic
		case ATM_BWR:		cmdtype = BWR;		packetLength = 2;	break;
		case ATM_P_BWR:		cmdtype = P_BWR;	packetLength = 2;	break;
		case ATM_BWR8R:		cmdtype = BWR8R;	packetLength = 2;	break;
		case ATM_SWAP16:	cmdtype = SWAP16;	packetLength = 2;	break;
			
		default:
			ERROR(header<<"   == Error - WRONG transaction type  (CurrentClock : "<<currentClockCycle<<")");
			ERROR(*tran);
			exit(0);
			break;
	}
	//packet, cmd, addr, cub, lng, *lat
	//#LogicLayer
	//Packet *newPacket = new Packet(REQUEST, cmdtype, tran->address, 0, packetLength, tran->trace);
	Packet *newPacket = new Packet(REQUEST, cmdtype, tran->address, 0, packetLength, tran->trace, tran->logicRequest);
	newPacket->reqDataSize = reqDataSize;
	return newPacket;
}

//
//Print current state in state log file
//
void LogicLayer::PrintState()
{
	int realInd = 0;
	if(downBuffers.size()>0) {
		STATEN(ALI(17)<<header);
		STATEN("Down ");
		for(int i=0; i<downBufferMax; i++) {
			if(i>0 && i%8==0) {
				STATEN(endl<<"                      ");
			}
			if(i < downBuffers.size()) {
				if(downBuffers[i] != NULL)	realInd = i;
				STATEN(*downBuffers[realInd]);
			}
			else if(i == downBufferMax-1) {
				STATEN("[ - ]");
			}
			else {
				STATEN("[ - ]...");
				break;
			}
		}
		STATEN(endl);
	}
	
	if(upBuffers.size()>0) {
		STATEN(ALI(17)<<header);
		STATEN(" Up  ");
		if(upBuffers.size() < upBufferMax) {
			for(int i=0; i<upBufferMax; i++) {
				if(i>0 && i%8==0) {
					STATEN(endl<<"                      ");
				}
				if(i < upBuffers.size()) {
					if(upBuffers[i] != NULL)	realInd = i;
					STATEN(*upBuffers[realInd]);
				}
				else if(i == upBufferMax-1) {
					STATEN("[ - ]");
				}
				else {
					STATEN("[ - ]...");
					break;
				}
			}
		}
		else {
			for(int i=0; i<upBuffers.size(); i++) {
				if(i>0 && i%8==0) {
					STATEN(endl<<"                      ");
				}
				if(upBuffers[i] != NULL)	realInd = i;
				STATEN(*upBuffers[realInd]);
			}
		}
		STATEN(endl);
	}
}

} //namespace CasHMC
