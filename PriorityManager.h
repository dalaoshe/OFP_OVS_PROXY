//
// Created by dalaoshe on 17-7-27.
//

#ifndef TCP_TUNNEL_PRIORITYMANAGER_H
#define TCP_TUNNEL_PRIORITYMANAGER_H

#include "RingBuffer.h"
#include <map>
struct Split{
    struct UEPID{
        uint8_t eid;
        uint8_t uid;
        bool operator < (const UEPID& id) const {
            return eid < id.eid || (eid == id.eid && uid < id.uid);
        }
        UEPID() {
            this->eid = this->uid = 0;
        }
    };
    std::map<Split::UEPID, uint32_t> uepMap;
    uint32_t totalNumberSplit;
    Split() {
        this->totalNumberSplit = 0;
    }

    bool hasUEP(Split::UEPID uep) {
        std::map<Split::UEPID , uint32_t >::iterator iter;
        iter = this->uepMap.find(uep);
        if(iter != this->uepMap.end()) return 1;
        return 0;
    }

    void addUEP(Split::UEPID uep, uint32_t number) {
        if(hasUEP(uep)) {
            this->uepMap[uep] = this->uepMap[uep] + number;
        }
        else {
            this->uepMap.insert(std::pair<Split::UEPID, uint32_t >(uep , number));
        }
        this->totalNumberSplit += number;
    }

    uint32_t getUEPNumber(Split::UEPID uep) {
        return this->uepMap[uep];
    }

    void init() {
        this->uepMap.clear();
        this->totalNumberSplit = 0;
    }

    Split operator + (const Split& b) const {
        Split temp;
        temp.totalNumberSplit = this->totalNumberSplit + b.totalNumberSplit;
        return temp;
    }

};

class PriorityManager {
    RingBuffer<Split>* windowBuffer;
    uint32_t  windowSize;
    uint32_t bufferSize;
public:
    PriorityManager(uint32_t windowSize, uint32_t bufferSize) {
        this->windowSize = windowSize;
        this->bufferSize = bufferSize;
        this->windowBuffer = new RingBuffer<Split>(this->bufferSize, this->windowSize);
    }

    void updateSplitOfUEP(uint32_t splitId, Split::UEPID uep, uint32_t number) {
        if(this->windowBuffer->aheadofHead(splitId)) {
            Split split;
            split.addUEP(uep, number);
            this->windowBuffer->putdData(split);
        }
        else {
            Split old = this->windowBuffer->getData(splitId);
            old.addUEP(uep, number);
            this->windowBuffer->updateData(splitId, old);
        }
    }

    double getUEPRatioOfSplitK(uint32_t splitId, Split::UEPID uep) {
        uint32_t UEP = ((Split)this->windowBuffer->getData(splitId)).getUEPNumber(uep);
        uint32_t TOTAL = this->windowBuffer->getTotal(splitId-this->windowSize, this->windowSize).totalNumberSplit;
        return (double)UEP / (double)TOTAL;
    }

    double convertRatioToPriority(double ratio) {
        //todo
        double priorit = 0;
        //...
        return priorit;
    }
};


#endif //TCP_TUNNEL_PRIORITYMANAGER_H
