//
// Created by dalaoshe on 17-7-27.
//

#ifndef TCP_TUNNEL_PRIORITYMANAGER_H
#define TCP_TUNNEL_PRIORITYMANAGER_H

#include "RingBuffer.h"
#include <map>
#include <math.h>
#include <pyport.h>

struct Split{
    struct UEPID{
        uint8_t uid;
        uint8_t eid;
        bool operator < (const UEPID& id) const {
            return eid < id.eid || (eid == id.eid && uid < id.uid);
        }
        UEPID() {
            this->eid = this->uid = 0;
        }
    }__attribute__((packed));
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
    timeval start;
public:
    PriorityManager(uint32_t windowSize, uint32_t bufferSize) {
        this->windowSize = windowSize;
        this->bufferSize = bufferSize;
        this->windowBuffer = new RingBuffer<Split>(this->bufferSize, this->windowSize);
        gettimeofday(&this->start, NULL);
    }

    void updateSplitOfUEP(uint32_t splitId, Split::UEPID uep, uint32_t number) {
        this->windowBuffer->printHeadTail(splitId);
        if ((!this->windowBuffer->aheadofHead(splitId)) && this->windowBuffer->aheadoftail(splitId)) {
            Split old = this->windowBuffer->getData(splitId);
            old.addUEP(uep, number);
            this->windowBuffer->updateData(splitId, old);
           fprintf(stderr, "UEP:[%02X,%02X] ADD window_id:%lu uep_window_number:%lu n_window_total:%lu \n", uep.eid, uep.uid, splitId, old.getUEPNumber(uep), old.totalNumberSplit);
        }
        else {

            Split split;
            split.addUEP(uep, number);
            if(!this->windowBuffer->aheadoftail(splitId))
                this->windowBuffer->updateHeadTail(splitId);
            this->windowBuffer->putdData(split);
           // fprintf(stderr, "UEP:[%02X,%02X] update window_id:%lu uep_window_number:%lu n_window_total:%lu \n", uep.eid, uep.uid, splitId, split.getUEPNumber(uep), split.totalNumberSplit);
        }

    }

    double getUEPRatioOfSplitK(uint32_t splitId, Split::UEPID uep) {
        Split split = ((Split)this->windowBuffer->getData(splitId));
        uint32_t UEP = ((Split)this->windowBuffer->getData(splitId)).getUEPNumber(uep);
        uint32_t TOTAL = this->windowBuffer->getTotal(splitId - this->windowSize + 1, this->windowSize).totalNumberSplit;
        fprintf(stderr, "UEP:[%02X,%02X] window_id:%lu uep_window_number:%lu n_window_total:%lu \n", uep.eid, uep.uid, splitId, UEP, TOTAL);
        double avg_ratio = 1.0 / split.uepMap.size();
        if(TOTAL == 0) return 0.0;
        //double ratio = (double)UEP / (double)TOTAL - avg_ratio + 0.2;
        double ratio = (double)UEP / ((double)TOTAL + 5.0);
        fprintf(stderr, "UEP:[%02X,%02X] window_id:%lu uep_window_number:%lu n_window_total:%lu priority:%lf\n", uep.eid, uep.uid, splitId, UEP, TOTAL, exp(-ratio));
        return ratio;

    }

    double convertRatioToPriority(double ratio) {
        //todo
        double priorit = exp(-ratio);
        //...

        return priorit;
    }

    double getUEPPriorityOfSplitK(uint32_t splitId, Split::UEPID uep) {
        double ratio = getUEPRatioOfSplitK(splitId, uep);
        return convertRatioToPriority(ratio);
    }

    bool isDangerous(double priority) {
        return priority < 0.5;
    }
    uint16_t getNowWindowId() {
        timeval end;
        gettimeofday(&end, NULL);
        double s = end.tv_sec - this->start.tv_sec;
        double us = end.tv_usec - this->start.tv_usec;
        double ms = s * 1000.0 + us / 1000.0;
        uint16_t wid = uint16_t((ms / 2000)) ;//% (this->bufferSize);
        return wid;
    }
};


#endif //TCP_TUNNEL_PRIORITYMANAGER_H
