#include "simulatorPro.h"


using namespace std;
using namespace boost;


Simulator_Pro_t::Simulator_Pro_t(Abc_Ntk_t * pNtk, int nFrame)
{
    DASSERT(Abc_NtkIsAigLogic(pNtk), "network is not in aig");
    this->pNtk = pNtk;
    this->nFrame = nFrame;
    this->nBlock = (nFrame % 64) ? ((nFrame >> 6) + 1) : (nFrame >> 6);
    this->nLastBlock = (nFrame % 64)? (nFrame % 64): 64;
    Abc_Obj_t * pObj = nullptr;
    int i = 0;
    int maxId = -1;
    Abc_NtkForEachObj(pNtk, pObj, i)
        maxId = max(maxId, pObj->Id);
    this->values.resize(maxId + 1);
    this->tmpValues.resize(maxId + 1);
    Abc_NtkForEachObj(pNtk, pObj, i) {
        this->values[pObj->Id].resize(nBlock);
        this->tmpValues[pObj->Id].resize(nBlock);
    }
}


Simulator_Pro_t::~Simulator_Pro_t()
{
}


void Simulator_Pro_t::Input(unsigned seed)
{
    // uniform distribution
    random::uniform_int_distribution <uint64_t> unf;
    random::mt19937 engine(seed);
    variate_generator <random::mt19937, random::uniform_int_distribution <uint64_t> > randomPI(engine, unf);

    // primary inputs
    Abc_Obj_t * pObj = nullptr;
    int k = 0;
    Abc_NtkForEachPi(pNtk, pObj, k) {
        for (int i = 0; i < nBlock; ++i) {
            values[pObj->Id][i] = randomPI();
            tmpValues[pObj->Id][i] = values[pObj->Id][i];
        }
    }

    // constant nodes
    DASSERT(Abc_NtkIsAigLogic(pNtk), "network is not in aig");
    Abc_NtkForEachNode(pNtk, pObj, k) {
        Hop_Obj_t * pHopObj = static_cast <Hop_Obj_t *> (pObj->pData);
        Hop_Obj_t * pHopObjR = Hop_Regular(pHopObj);
        if (Hop_ObjIsConst1(pHopObjR)) {
            DASSERT(Hop_ObjFanin0(pHopObjR) == nullptr);
            DASSERT(Hop_ObjFanin1(pHopObjR) == nullptr);
            if (!Hop_IsComplement(pHopObj)) {
                for (int i = 0; i < nBlock; ++i) {
                    values[pObj->Id][i] = static_cast <uint64_t> (ULLONG_MAX);
                    tmpValues[pObj->Id][i] = static_cast <uint64_t> (ULLONG_MAX);
                }
            }
            else {
                for (int i = 0; i < nBlock; ++i) {
                    values[pObj->Id][i] = 0;
                    tmpValues[pObj->Id][i] = 0;
                }
            }
        }
    }
}


void Simulator_Pro_t::Input(string fileName)
{
    // primary inputs
    FILE * fp = fopen(fileName.c_str(), "r");
    char buf[1000];
    int cnt = 0;
    while (fgets(buf, sizeof(buf), fp) != nullptr) {
        DASSERT(static_cast <int>(strlen(buf)) == Abc_NtkPiNum(pNtk) + 3);
        int blockId = cnt >> 6;
        int bitId = cnt % 64;
        for (int i = 0; i < Abc_NtkPiNum(pNtk); ++i) {
            Abc_Obj_t * pObj = Abc_NtkPi(pNtk, i);
            if (buf[i + 2] == '1')
                Ckt_SetBit( values[pObj->Id][blockId], bitId );
            else
                Ckt_ResetBit( values[pObj->Id][blockId], bitId );
        }
        ++cnt;
        DASSERT(cnt <= nFrame);
    }
    DASSERT(cnt == nFrame);
    fclose(fp);

    // constant nodes
    DASSERT(Abc_NtkIsAigLogic(pNtk), "network is not in aig");
    Abc_Obj_t * pObj = nullptr;
    int k = 0;
    Abc_NtkForEachNode(pNtk, pObj, k) {
        Hop_Obj_t * pHopObj = static_cast <Hop_Obj_t *> (pObj->pData);
        Hop_Obj_t * pHopObjR = Hop_Regular(pHopObj);
        if (Hop_ObjIsConst1(pHopObjR)) {
            DASSERT(Hop_ObjFanin0(pHopObjR) == nullptr);
            DASSERT(Hop_ObjFanin1(pHopObjR) == nullptr);
            if (!Hop_IsComplement(pHopObj)) {
                for (int i = 0; i < nBlock; ++i) {
                    values[pObj->Id][i] = static_cast <uint64_t> (ULLONG_MAX);
                    tmpValues[pObj->Id][i] = static_cast <uint64_t> (ULLONG_MAX);
                }
            }
            else {
                for (int i = 0; i < nBlock; ++i) {
                    values[pObj->Id][i] = 0;
                    tmpValues[pObj->Id][i] = 0;
                }
            }
        }
    }
}


void Simulator_Pro_t::Simulate()
{
    Abc_Obj_t * pObj = nullptr;
    int i = 0;
    Vec_Ptr_t * vNodes = Abc_NtkDfs(pNtk, 0);
    // internal nodes
    Vec_PtrForEachEntry(Abc_Obj_t *, vNodes, pObj, i)
        UpdateAigNode(pObj);
    Vec_PtrFree(vNodes);
    // primary outputs
    Abc_NtkForEachPo(pNtk, pObj, i) {
        Abc_Obj_t * pFanin = Abc_ObjFanin0(pObj);
        values[pObj->Id].assign(values[pFanin->Id].begin(), values[pFanin->Id].end());
    }
}


void Simulator_Pro_t::SimulateResub(Abc_Obj_t * pOldObj, Hop_Obj_t * pResubFunc, Vec_Ptr_t * vResubFanins)
{
    Abc_Obj_t * pObj = nullptr;
    int i = 0;
    Vec_Ptr_t * vNodes = Ckt_NtkDfsResub(pNtk, pOldObj, vResubFanins);
    // internal nodes
    Vec_PtrForEachEntry(Abc_Obj_t *, vNodes, pObj, i) {
        if (pObj != pOldObj)
            UpdateAigNodeResub(pObj, nullptr, nullptr);
        else
            UpdateAigNodeResub(pObj, pResubFunc, vResubFanins);
    }
    // primary outputs
    Abc_NtkForEachPo(pNtk, pObj, i) {
        Abc_Obj_t * pFanin = Abc_ObjFanin0(pObj);
        tmpValues[pObj->Id].assign(tmpValues[pFanin->Id].begin(), tmpValues[pFanin->Id].end());
    }
    Vec_PtrFree(vNodes);
}


void Simulator_Pro_t::UpdateAigNode(Abc_Obj_t * pObj)
{
    Hop_Man_t * pMan = static_cast <Hop_Man_t *> (pNtk->pManFunc);
    Hop_Obj_t * pRoot = static_cast <Hop_Obj_t *> (pObj->pData);
    Hop_Obj_t * pRootR = Hop_Regular(pRoot);

    // skip constant node
    if (Hop_ObjIsConst1(pRootR))
        return;

    // get topological order of subnetwork in aig
    Vec_Ptr_t * vHopNodes = Hop_ManDfsNode(pMan, pRootR);

    // init internal hop nodes
    int maxHopId = -1;
    int i = 0;
    Hop_Obj_t * pHopObj = nullptr;
    Vec_PtrForEachEntry(Hop_Obj_t *, vHopNodes, pHopObj, i)
        maxHopId = max(maxHopId, pHopObj->Id);
    Hop_ManForEachPi(pMan, pHopObj, i)
        maxHopId = max(maxHopId, pHopObj->Id);
    vector < tVec > interValues(maxHopId + 1);
    Vec_PtrForEachEntry(Hop_Obj_t *, vHopNodes, pHopObj, i)
        interValues[pHopObj->Id].resize(nBlock);
    unordered_map <int, tVec *> hop2Val;
    Abc_Obj_t * pFanin = nullptr;
    Abc_ObjForEachFanin(pObj, pFanin, i)
        hop2Val[Hop_ManPi(pMan, i)->Id] = &values[pFanin->Id];

    // special case for inverter or buffer
    if (pRootR->Type == AIG_PI) {
        pFanin = Abc_ObjFanin0(pObj);
        values[pObj->Id].assign(values[pFanin->Id].begin(), values[pFanin->Id].end());
    }

    // simulate
    Vec_PtrForEachEntry(Hop_Obj_t *, vHopNodes, pHopObj, i) {
        DASSERT(Hop_ObjIsAnd(pHopObj));
        Hop_Obj_t * pHopFanin0 = Hop_ObjFanin0(pHopObj);
        Hop_Obj_t * pHopFanin1 = Hop_ObjFanin1(pHopObj);
        DASSERT(!Hop_ObjIsConst1(pHopFanin0));
        DASSERT(!Hop_ObjIsConst1(pHopFanin1));
        tVec & val0 = Hop_ObjIsPi(pHopFanin0) ? *hop2Val[pHopFanin0->Id] : interValues[pHopFanin0->Id];
        tVec & val1 = Hop_ObjIsPi(pHopFanin1) ? *hop2Val[pHopFanin1->Id] : interValues[pHopFanin1->Id];
        tVec & out = (pHopObj == pRootR) ? values[pObj->Id] : interValues[pHopObj->Id];
        bool isFanin0C = Hop_ObjFaninC0(pHopObj);
        bool isFanin1C = Hop_ObjFaninC1(pHopObj);
        if (!isFanin0C && !isFanin1C) {
            for (int j = 0; j < nBlock; ++j)
                out[j] = val0[j] & val1[j];
        }
        else if (!isFanin0C && isFanin1C) {
            for (int j = 0; j < nBlock; ++j)
                out[j] = val0[j] & (~val1[j]);
        }
        else if (isFanin0C && !isFanin1C) {
            for (int j = 0; j < nBlock; ++j)
                out[j] = (~val0[j]) & val1[j];
        }
        else if (isFanin0C && isFanin1C) {
            for (int j = 0; j < nBlock; ++j)
                out[j] = ~(val0[j] | val1[j]);
        }
    }

    // complement
    if (Hop_IsComplement(pRoot)) {
        for (int j = 0; j < nBlock; ++j)
            values[pObj->Id][j] ^= static_cast <uint64_t> (ULLONG_MAX);
    }

    // recycle memory
    Vec_PtrFree(vHopNodes);
}


void Simulator_Pro_t::UpdateAigNodeResub(Abc_Obj_t * pObj, Hop_Obj_t * pResubFunc, Vec_Ptr_t * vResubFanins)
{
    Hop_Man_t * pMan = static_cast <Hop_Man_t *> (pNtk->pManFunc);
    Hop_Obj_t * pRoot = (pResubFunc == nullptr) ? static_cast <Hop_Obj_t *> (pObj->pData): pResubFunc;
    Hop_Obj_t * pRootR = Hop_Regular(pRoot);

    // update constant node
    if (pResubFunc == nullptr) {
        if (pRootR->Type == AIG_CONST1)
            return;
    }
    else {
        if (Hop_ObjIsConst1(pRootR)) {
            DASSERT(Hop_ObjFanin0(pRootR) == nullptr);
            DASSERT(Hop_ObjFanin1(pRootR) == nullptr);
            if (!Hop_IsComplement(pRoot)) {
                for (int i = 0; i < nBlock; ++i)
                    tmpValues[pObj->Id][i] = static_cast <uint64_t> (ULLONG_MAX);
                return;
            }
            else {
                for (int i = 0; i < nBlock; ++i)
                    tmpValues[pObj->Id][i] = 0;
                return;
            }
        }
    }

    // get topological order of subnetwork in aig
    Vec_Ptr_t * vHopNodes = Hop_ManDfsNode(pMan, pRootR);

    // init internal hop nodes
    int maxHopId = -1;
    int i = 0;
    Hop_Obj_t * pHopObj = nullptr;
    Vec_PtrForEachEntry(Hop_Obj_t *, vHopNodes, pHopObj, i)
        maxHopId = max(maxHopId, pHopObj->Id);
    Hop_ManForEachPi(pMan, pHopObj, i)
        maxHopId = max(maxHopId, pHopObj->Id);
    vector < tVec > interValues(maxHopId + 1);
    Vec_PtrForEachEntry(Hop_Obj_t *, vHopNodes, pHopObj, i)
        interValues[pHopObj->Id].resize(nBlock);
    unordered_map <int, tVec *> hop2Val;
    Abc_Obj_t * pFanin = nullptr;
    if (pResubFunc != nullptr) {
        Vec_PtrForEachEntry(Abc_Obj_t *, vResubFanins, pFanin, i) {
            hop2Val[Hop_ManPi(pMan, i)->Id] = &tmpValues[pFanin->Id];
        }
    }
    else {
        Abc_ObjForEachFanin(pObj, pFanin, i)
            hop2Val[Hop_ManPi(pMan, i)->Id] = &tmpValues[pFanin->Id];
    }

    // special case for inverter or buffer
    if (pRootR->Type == AIG_PI) {
        DASSERT(Vec_PtrSize(vHopNodes) == 0);
        if (pResubFunc == nullptr)
            pFanin = Abc_ObjFanin0(pObj);
        else {
            pFanin = static_cast <Abc_Obj_t *> (Vec_PtrEntry(vResubFanins, 0));
        }
        tmpValues[pObj->Id].assign(tmpValues[pFanin->Id].begin(), tmpValues[pFanin->Id].end());
    }

    // simulate
    Vec_PtrForEachEntry(Hop_Obj_t *, vHopNodes, pHopObj, i) {
        DASSERT(Hop_ObjIsAnd(pHopObj));
        Hop_Obj_t * pHopFanin0 = Hop_ObjFanin0(pHopObj);
        Hop_Obj_t * pHopFanin1 = Hop_ObjFanin1(pHopObj);
        DASSERT(!Hop_ObjIsConst1(pHopFanin0));
        DASSERT(!Hop_ObjIsConst1(pHopFanin1));
        tVec & val0 = Hop_ObjIsPi(pHopFanin0) ? *hop2Val[pHopFanin0->Id] : interValues[pHopFanin0->Id];
        tVec & val1 = Hop_ObjIsPi(pHopFanin1) ? *hop2Val[pHopFanin1->Id] : interValues[pHopFanin1->Id];
        tVec & out = (pHopObj == pRootR) ? tmpValues[pObj->Id] : interValues[pHopObj->Id];
        bool isFanin0C = Hop_ObjFaninC0(pHopObj);
        bool isFanin1C = Hop_ObjFaninC1(pHopObj);
        if (!isFanin0C && !isFanin1C) {
            for (int j = 0; j < nBlock; ++j)
                out[j] = val0[j] & val1[j];
        }
        else if (!isFanin0C && isFanin1C) {
            for (int j = 0; j < nBlock; ++j)
                out[j] = val0[j] & (~val1[j]);
        }
        else if (isFanin0C && !isFanin1C) {
            for (int j = 0; j < nBlock; ++j)
                out[j] = (~val0[j]) & val1[j];
        }
        else if (isFanin0C && isFanin1C) {
            for (int j = 0; j < nBlock; ++j)
                out[j] = ~(val0[j] | val1[j]);
        }
    }

    // complement
    if (Hop_IsComplement(pRoot)) {
        for (int j = 0; j < nBlock; ++j)
            tmpValues[pObj->Id][j] ^= static_cast <uint64_t> (ULLONG_MAX);
    }

    // recycle memory
    Vec_PtrFree(vHopNodes);
}


multiprecision::int256_t Simulator_Pro_t::GetInput(int lsb, int msb, int frameId) const
{
    DASSERT(lsb >= 0 && msb < Abc_NtkPiNum(pNtk));
    DASSERT(frameId <= nFrame);
    DASSERT(lsb <= msb && msb - lsb <= 256);
    multiprecision::int256_t ret(0);
    int blockId = frameId >> 6;
    int bitId = frameId % 64;
    for (int k = msb; k >= lsb; --k) {
        Abc_Obj_t * pObj = Abc_NtkPi(pNtk, k);
        if (GetValue(pObj, blockId, bitId))
            ++ret;
        ret <<= 1;
    }
    return ret;
}


multiprecision::int256_t Simulator_Pro_t::GetOutput(int lsb, int msb, int frameId, bool isTmpValue) const
{
    DASSERT(lsb >= 0 && msb < Abc_NtkPoNum(pNtk));
    DASSERT(frameId <= nFrame);
    DASSERT(lsb <= msb && msb - lsb <= 256);
    multiprecision::int256_t ret(0);
    int blockId = frameId >> 6;
    int bitId = frameId % 64;
    for (int k = msb; k >= lsb; --k) {
        ret <<= 1;
        Abc_Obj_t * pObj = Abc_NtkPo(pNtk, k);
        if (!isTmpValue) {
            if (GetValue(pObj, blockId, bitId))
                ++ret;
        }
        else {
            if (GetTmpValue(pObj, blockId, bitId))
                ++ret;
        }
    }
    return ret;
}


void Simulator_Pro_t::PrintInputStream(int frameId) const
{
    Abc_Obj_t * pObj = nullptr;
    int i = 0;
    int blockId = frameId >> 6;
    int bitId = frameId % 64;
    Abc_NtkForEachPi(pNtk, pObj, i)
        cout << GetValue(pObj, blockId, bitId);
    cout << endl;
}


void Simulator_Pro_t::PrintOutputStream(int frameId) const
{
    Abc_Obj_t * pObj = nullptr;
    int i = 0;
    int blockId = frameId >> 6;
    int bitId = frameId % 64;
    Abc_NtkForEachPo(pNtk, pObj, i)
        cout << GetValue(pObj, blockId, bitId);
    cout << endl;
}


double MeasureAEMR(Abc_Ntk_t * pNtk1, Abc_Ntk_t * pNtk2, int nFrame, unsigned seed, bool isCheck)
{
    // check PI/PO
    if (isCheck)
        DASSERT(IOChecker(pNtk1, pNtk2));

    // simulation
    Simulator_Pro_t smlt1(pNtk1, nFrame);
    smlt1.Input(seed);
    smlt1.Simulate();
    Simulator_Pro_t smlt2(pNtk2, nFrame);
    smlt2.Input(seed);
    smlt2.Simulate();

    // compute
    return GetAEMR(&smlt1, &smlt2, false, false);
}


double MeasureResubAEMR(Simulator_Pro_t * pSmlt1, Simulator_Pro_t * pSmlt2, Abc_Obj_t * pOldObj, Hop_Obj_t * pResubFunc, Vec_Ptr_t * vResubFanins, bool isCheck)
{
    if (isCheck)
        DASSERT(SmltChecker(pSmlt1, pSmlt2));
    pSmlt2->SimulateResub(pOldObj, pResubFunc, vResubFanins);
    return GetAEMR(pSmlt1, pSmlt2, false, true);
}


double GetAEMR(Simulator_Pro_t * pSmlt1, Simulator_Pro_t * pSmlt2, bool isCheck, bool isResub)
{
    if (isCheck)
        DASSERT(SmltChecker(pSmlt1, pSmlt2));
    typedef multiprecision::cpp_dec_float_100 bigFlt;
    typedef multiprecision::int256_t bigInt;
    bigInt sum(0);
    int nPo = Abc_NtkPoNum(pSmlt1->GetNetwork());
    int nFrame = pSmlt1->GetFrameNum();
    if (isResub) {
        for (int k = 0; k < nFrame; ++k)
            sum += (abs(pSmlt1->GetOutput(0, nPo - 1, k, 0) - pSmlt2->GetOutput(0, nPo - 1, k, 1)));
    }
    else {
        for (int k = 0; k < nFrame; ++k)
            sum += (abs(pSmlt1->GetOutput(0, nPo - 1, k, 0) - pSmlt2->GetOutput(0, nPo - 1, k, 0)));
    }
    bigInt frac = (static_cast <bigInt> (nFrame)) << nPo;
    return static_cast <double> (static_cast <bigFlt>(sum) / static_cast <bigFlt>(frac));
}


double MeasureER(Abc_Ntk_t * pNtk1, Abc_Ntk_t * pNtk2, int nFrame, unsigned seed, bool isCheck)
{
    if (isCheck)
        DASSERT(IOChecker(pNtk1, pNtk2));

    // simulation
    Simulator_Pro_t smlt1(pNtk1, nFrame);
    smlt1.Input(seed);
    smlt1.Simulate();
    Simulator_Pro_t smlt2(pNtk2, nFrame);
    smlt2.Input(seed);
    smlt2.Simulate();

    // compute
    return GetER(&smlt1, &smlt2, false, false);
}


double MeasureResubER(Simulator_Pro_t * pSmlt1, Simulator_Pro_t * pSmlt2, Abc_Obj_t * pOldObj, Hop_Obj_t * pResubFunc, Vec_Ptr_t * vResubFanins, bool isCheck)
{
    if (isCheck)
        DASSERT(SmltChecker(pSmlt1, pSmlt2));
    pSmlt2->SimulateResub(pOldObj, pResubFunc, vResubFanins);
    return GetER(pSmlt1, pSmlt2, false, true);
}


double GetER(Simulator_Pro_t * pSmlt1, Simulator_Pro_t * pSmlt2, bool isCheck, bool isResub)
{
    if (isCheck)
        DASSERT(SmltChecker(pSmlt1, pSmlt2));

    int ret = 0;
    Abc_Ntk_t * pNtk1 = pSmlt1->GetNetwork();
    Abc_Ntk_t * pNtk2 = pSmlt2->GetNetwork();
    int nPo = Abc_NtkPoNum(pNtk1);
    int nBlock = pSmlt1->GetBlockNum();
    int nLastBlock = pSmlt1->GetLastBlockLen();
    vector <tVec> * pValues1 = pSmlt1->GetPValues();
    vector <tVec> * pValues2 = nullptr;
    if (!isResub)
         pValues2 = pSmlt2->GetPValues();
    else
         pValues2 = pSmlt2->GetPTmpValues();
    for (int k = 0; k < nBlock; ++k) {
        uint64_t temp = 0;
        for (int i = 0; i < nPo; ++i)
            temp |= (*pValues1)[Abc_NtkPo(pNtk1, i)->Id][k] ^
                    (*pValues2)[Abc_NtkPo(pNtk2, i)->Id][k];
        if (k == nBlock - 1) {
            temp >>= (64 - nLastBlock);
            temp <<= (64 - nLastBlock);
        }
        ret += Ckt_CountOneNum(temp);
    }
    return ret / static_cast<double> (pSmlt1->GetFrameNum());
}


bool IOChecker(Abc_Ntk_t * pNtk1, Abc_Ntk_t * pNtk2)
{
    int nPo = Abc_NtkPoNum(pNtk1);
    if (nPo != Abc_NtkPoNum(pNtk2))
        return false;
    for (int i = 0; i < nPo; ++i)
        if (strcmp(Abc_ObjName(Abc_NtkPo(pNtk1, i)), Abc_ObjName(Abc_NtkPo(pNtk2, i))) != 0)
            return false;
    int nPi = Abc_NtkPiNum(pNtk1);
    if (nPi != Abc_NtkPiNum(pNtk2))
        return false;
    for (int i = 0; i < nPi; ++i)
        if (strcmp(Abc_ObjName(Abc_NtkPi(pNtk1, i)), Abc_ObjName(Abc_NtkPi(pNtk2, i))) != 0)
            return false;
    return true;
}


bool SmltChecker(Simulator_Pro_t * pSmlt1, Simulator_Pro_t * pSmlt2)
{
    if (!IOChecker(pSmlt1->GetNetwork(), pSmlt2->GetNetwork()))
        return false;
    if (pSmlt1->GetFrameNum() != pSmlt2->GetFrameNum())
        return false;
    return true;
}


Vec_Ptr_t * Ckt_NtkDfsResub(Abc_Ntk_t * pNtk, Abc_Obj_t * pObjOld, Vec_Ptr_t * vFanins)
{
    Vec_Ptr_t * vNodes;
    Abc_Obj_t * pObj;
    int i;
    // set the traversal ID
    Abc_NtkIncrementTravId( pNtk );
    // start the array of nodes
    vNodes = Vec_PtrAlloc( 100 );
    Abc_NtkForEachCo( pNtk, pObj, i )
    {
        Abc_NodeSetTravIdCurrent( pObj );
        Ckt_NtkDfsResub_rec(Abc_ObjFanin0(pObj), vNodes, pObjOld, vFanins);
    }
    return vNodes;
}


void Ckt_NtkDfsResub_rec(Abc_Obj_t * pNode, Vec_Ptr_t * vNodes, Abc_Obj_t * pObjOld, Vec_Ptr_t * vFanins)
{
    Abc_Obj_t * pFanin;
    int i;
    assert( !Abc_ObjIsNet(pNode) );
    // if this node is already visited, skip
    if ( Abc_NodeIsTravIdCurrent( pNode ) )
        return;
    // mark the node as visited
    Abc_NodeSetTravIdCurrent( pNode );
    // skip the CI
    if ( Abc_ObjIsCi(pNode) || (Abc_NtkIsStrash(pNode->pNtk) && Abc_AigNodeIsConst(pNode)) )
        return;
    assert( Abc_ObjIsNode( pNode ) || Abc_ObjIsBox( pNode ) );
    // visit the transitive fanin of the node
    if (pNode != pObjOld) {
        Abc_ObjForEachFanin( pNode, pFanin, i )
            Ckt_NtkDfsResub_rec(pFanin, vNodes, pObjOld, vFanins);
    }
    else {
        Vec_PtrForEachEntry(Abc_Obj_t *, vFanins, pFanin, i)
            Ckt_NtkDfsResub_rec(pFanin, vNodes, pObjOld, vFanins);
    }
    // add the node after the fanins have been added
    Vec_PtrPush( vNodes, pNode );
}
