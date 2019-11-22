#ifndef DCALS_H
#define DCALS_H


#include <boost/progress.hpp>
#include <boost/random.hpp>
#include "simulatorPro.h"
#include "cktUtil.h"


class Dcals_Man_t
{
private:
    Abc_Ntk_t * pOriNtk;
    Abc_Ntk_t * pAppNtk;
    Simulator_Pro_t * pOriSmlt;
    Simulator_Pro_t * pAppSmlt;
    unsigned seed;
    Metric_t metricType;
    int mapType;
    int nFrame;
    int cutSize;
    int nEvalFrame;
    int nEstiFrame;
    double metric;
    double metricBound;
    double maxDelay;
    Mfs_Par_t * pPars;
    std::string outPath;

    Dcals_Man_t & operator = (const Dcals_Man_t &);
    Dcals_Man_t(const Dcals_Man_t &);

public:
    explicit Dcals_Man_t(Abc_Ntk_t * pNtk, int nFrame, int cutSize, double metricBound, Metric_t metricType, int mapType = 0, std::string outPath = "appntk/");
    ~Dcals_Man_t();
    Mfs_Par_t * InitMfsPars();
    void DCALS();
    void LocalAppChange();
    Hop_Obj_t * LocalAppChangeNode(Mfs_Man_t * p, Abc_Obj_t * pNode);
    Aig_Man_t * ConstructAppAig(Mfs_Man_t * p, Abc_Obj_t * pNode);
    void GenCand(IN bool genConst, INOUT std::vector <Lac_Cand_t> & cands);
    void BatchErrorEst(IN std::vector <Lac_Cand_t> & cands, OUT Lac_Cand_t & bestCand);
};


Aig_Obj_t * Abc_NtkConstructAig_rec(Mfs_Man_t * p, Abc_Obj_t * pNode, Aig_Man_t * pMan);
void Abc_MfsConvertHopToAig(Abc_Obj_t * pObjOld, Aig_Man_t * pMan);
void Abc_MfsConvertHopToAig_rec(Hop_Obj_t * pObj, Aig_Man_t * pMan);
Vec_Ptr_t * Ckt_FindCut(Abc_Obj_t * pNode, int nMax);
Hop_Obj_t * Ckt_NtkMfsResubNode(Mfs_Man_t * p, Abc_Obj_t * pNode);
Hop_Obj_t * Ckt_NtkMfsSolveSatResub(Mfs_Man_t * p, Abc_Obj_t * pNode, int iFanin, int fOnlyRemove);
int Ckt_NtkMfsTryResubOnce(Mfs_Man_t * p, int * pCands, int nCands);
void Ckt_NtkMfsUpdateNetwork(Mfs_Man_t * p, Abc_Obj_t * pObj, Vec_Ptr_t * vMfsFanins, Hop_Obj_t * pFunc);
void Ckt_UpdateNetwork(Abc_Obj_t * pObj, Vec_Ptr_t * vFanins, Hop_Obj_t * pFunc);
bool IsSimpPo(Abc_Obj_t * pObj);
Abc_Obj_t * GetFirstPoFanout(Abc_Obj_t * pObj);
Vec_Ptr_t * GetTFICone(Abc_Ntk_t * pNtk, Abc_Obj_t * pObj);

extern "C" {void Abc_NtkDfs_rec( Abc_Obj_t * pNode, Vec_Ptr_t * vNodes );}


#endif
