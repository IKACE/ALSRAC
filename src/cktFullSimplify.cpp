#include "cktFullSimplify.h"

using namespace std;
using namespace abc;
using namespace cmdline;


parser Cmdline_Parser(int argc, char * argv[])
{
    parser option;
    option.add <string>    ("input",   'i', "Original Circuit file",    true);
    option.add <string>    ("output",  'o', "Approximate Circuit file", false);
    option.add <string>    ("genlib",  'g', "Map libarary file",        false, "data/genlib/mcnc.genlib");
    option.add <int>       ("nFrame",  'f', "Simulation Frame number",  false, 10240, range(1, INT_MAX));
    option.add <int>       ("level",   'l', "TFO level",                false, 3,     range(0, INT_MAX));
    option.add <int>       ("nLocalPI",'m', "Local PI number",          false, 10,    range(1, INT_MAX));
    option.parse_check(argc, argv);
    return option;
}


void Ckt_FullSimplifyTest(int argc, char * argv[])
{
    parser option = Cmdline_Parser(argc, argv);
    string input = option.get <string> ("input");
    string output = option.get <string> ("output");
    string genlib = option.get <string> ("genlib");
    int nFrame = option.get <int> ("nFrame");
    int level = option.get <int> ("level");
    int nLocalPI = option.get <int> ("nLocalPI");

    Abc_Start();

    Abc_Frame_t * pAbc = Abc_FrameGetGlobalFrame();
    string command = "read_genlib -v " + genlib;
    DASSERT(Cmd_CommandExecute(pAbc, command.c_str()) == 0);
    command = "read_blif " + input;
    DASSERT(Cmd_CommandExecute(pAbc, command.c_str()) == 0);
    shared_ptr <Ckt_Ntk_t> pCktNtk = make_shared <Ckt_Ntk_t> (Abc_FrameReadNtk(pAbc), false);
    // Ckt_Visualize(pCktNtk->GetAbcNtk(), "test.dot");

    pCktNtk->Init(nFrame);
    pCktNtk->LogicSim(false);

    vector < shared_ptr <Ckt_Obj_t> > vRoots;
    vector < shared_ptr <Ckt_Obj_t> > vSupp;
    vector < shared_ptr <Ckt_Obj_t> > vNodes;
    DASSERT(system("if [ ! -d tmp ]; then mkdir tmp; fi") != -1);
    DASSERT(system("rm tmp/*") != -1);
    for (int i = 0; i < pCktNtk->GetObjNum(); ++i) {
        shared_ptr <Ckt_Obj_t> pPivot = pCktNtk->GetObj(i);
        cout << pPivot << endl;
        if (pPivot->IsPIO())
            continue;
        Ckt_ComputeRoot(pPivot, vRoots, level, nLocalPI);
        Ckt_ComputeSupport(vRoots, vSupp, nLocalPI);
        Ckt_CollectNodes(vRoots, vSupp, vNodes);
        Ckt_GenerateNtk(vRoots, vSupp, vNodes, "./tmp/" + pPivot->GetName() + ".blif");
    }

    Abc_Stop();
}


void Ckt_ComputeRoot(shared_ptr <Ckt_Obj_t> pCktObj, vector < shared_ptr <Ckt_Obj_t> > & vRoots, int nTfoLevel, int nMaxFO)
{
    vRoots.clear();
    DASSERT(!pCktObj->IsPO(), "Error: cannot traverse the fanout cone of PO");
    pCktObj->GetCktNtk()->SetUnvisited();
    Ckt_ComputeRoot_Rec(pCktObj, vRoots, nTfoLevel, nMaxFO);
}


void Ckt_ComputeRoot_Rec(shared_ptr <Ckt_Obj_t> pCktObj, vector < shared_ptr <Ckt_Obj_t> > & vRoots, int nTfoLevel, int nMaxFO)
{
    if (pCktObj->GetVisited())
        return;
    pCktObj->SetVisited();
    if (Ckt_CheckRoot(pCktObj, nTfoLevel, nMaxFO))
        vRoots.emplace_back(pCktObj);
    else
        for (int i = 0; i < pCktObj->GetFanoutNum(); ++i)
            Ckt_ComputeRoot_Rec(pCktObj->GetFanout(i), vRoots, nTfoLevel - 1, nMaxFO);
}


bool Ckt_CheckRoot(shared_ptr <Ckt_Obj_t> pCktObj, int nTfoLevel, int nMaxFO)
{
    if (nTfoLevel == 0)
        return true;
    if (pCktObj->GetFanoutNum() > nMaxFO)
        return true;
    for (int i = 0; i < pCktObj->GetFanoutNum(); ++i)
        if (pCktObj->GetFanout(i)->IsPO())
            return true;
    return false;
}


void Ckt_ComputeSupport(vector < shared_ptr <Ckt_Obj_t> > & vRoots, vector < shared_ptr <Ckt_Obj_t> > & vSupp, int nLocalPI)
{
    DASSERT(static_cast <int> (vRoots.size()) <= nLocalPI, "Error: the number of roots must be smaller than the number of local PI!");
    vSupp.clear();
    deque < shared_ptr <Ckt_Obj_t> > fringe;
    if (vRoots.size())
        vRoots[0]->GetCktNtk()->SetUnvisited();
    else
        return;
    for (auto & pCktObj : vRoots) {
        if (!pCktObj->GetVisited()) {
            pCktObj->SetVisited();
            fringe.emplace_back(pCktObj);
        }
    }
    while (fringe.size() && static_cast <int>(fringe.size()) < nLocalPI) {
        shared_ptr <Ckt_Obj_t> pCktObj = fringe.front();
        // cout << "pop " << pCktObj << endl;
        if (pCktObj->IsPI() || pCktObj->IsConst())
            vSupp.emplace_back(pCktObj);
        for (int i = 0; i < pCktObj->GetFaninNum(); ++i) {
            shared_ptr <Ckt_Obj_t> pFanin = pCktObj->GetFanin(i);
            if (!pFanin->GetVisited()) {
                pFanin->SetVisited();
                fringe.emplace_back(pFanin);
                // cout << "push " << pFanin << endl;
            }
        }
        fringe.pop_front();
    }
    for (auto & pCktObj: fringe) {
        if (static_cast <int> (vSupp.size()) < nLocalPI)
            vSupp.emplace_back(pCktObj);
        else
            break;
    }
}


void Ckt_CollectNodes(vector < shared_ptr <Ckt_Obj_t> > & vRoots, vector < shared_ptr <Ckt_Obj_t> > & vSupp, vector < shared_ptr <Ckt_Obj_t> > & vNodes)
{
    vNodes.clear();
    set < shared_ptr <Ckt_Obj_t> > vSuppSet(vSupp.begin(), vSupp.end());
    DASSERT(vSupp.size() == vSuppSet.size(), "Warning : there are identical elements in the local inputs!");
    if (vRoots.size())
        vRoots[0]->GetCktNtk()->SetUnvisited();
    else
        return;
    for (auto & pCktObj : vRoots) {
        DASSERT(!pCktObj->IsPO(), "Warning : collect nodes from PO!");
        Ckt_CollectNodes_Rec(pCktObj, vSuppSet, vNodes);
    }
}


void Ckt_CollectNodes_Rec(shared_ptr <Ckt_Obj_t> pCktObj, set < shared_ptr <Ckt_Obj_t> > & vSuppSet, vector < shared_ptr <Ckt_Obj_t> > & vNodes)
{
    if (pCktObj->GetVisited())
        return;
    if (vSuppSet.count(pCktObj))
        return;
    pCktObj->SetVisited();
    for (int i = 0; i < pCktObj->GetFaninNum(); ++i)
        Ckt_CollectNodes_Rec(pCktObj->GetFanin(i), vSuppSet, vNodes);

    vNodes.emplace_back(pCktObj);
}


void Ckt_GenerateNtk(vector < shared_ptr <Ckt_Obj_t> > & vRoots, vector < shared_ptr <Ckt_Obj_t> > & vSupp, vector < shared_ptr <Ckt_Obj_t> > & vNodes, string fileName)
{
    DASSERT(vSupp.size());
    DASSERT(vRoots.size());

    Abc_Ntk_t * pWinNtk = Abc_NtkAlloc(ABC_NTK_LOGIC, ABC_FUNC_SOP, 1);

    for (auto & pCktObj: vSupp) {
        // cout << "adding pis " << pCktObj << endl;
        Abc_Obj_t * pOldObj = pCktObj->GetAbcObj();
        Abc_Obj_t * pNewObj = Abc_NtkCreatePi(pWinNtk);
        Abc_ObjAssignName(pNewObj, Abc_ObjName(pOldObj), nullptr);
        pOldObj->pCopy = pNewObj;
    }

    for (auto & pCktObj: vNodes) {
        // cout << "copying nodes " << pCktObj << endl;
        Abc_Obj_t * pOldObj = pCktObj->GetAbcObj();
        Abc_Obj_t * pNewObj = Abc_NtkDupObj(pWinNtk, pOldObj, 0);
        Abc_ObjAssignName(pNewObj, Abc_ObjName(pOldObj), nullptr);
        DASSERT(pNewObj == Abc_ObjCopy(pOldObj), "Error: the copy of the original object is not exactly the current object!");
        Abc_Obj_t * pFanin = nullptr;
        int i = 0;
        Abc_ObjForEachFanin(pOldObj, pFanin, i) {
            DASSERT(pOldObj != nullptr && pFanin != nullptr);
            Abc_ObjAddFanin(Abc_ObjCopy(pOldObj), Abc_ObjCopy(pFanin));
        }
    }

    for (auto & pCktObj: vRoots) {
        // cout << "adding pos " << pCktObj << endl;
        Abc_Obj_t * pOldObj = pCktObj->GetAbcObj();
        Abc_Obj_t * pNewPo = Abc_NtkCreatePo(pWinNtk);
        Abc_ObjAssignName(pNewPo, Abc_ObjName(pOldObj), nullptr);
        Abc_ObjAddFanin(pNewPo, Abc_ObjCopy(pOldObj));
    }

    Io_Write(pWinNtk, const_cast <char *>(fileName.c_str()), IO_FILE_BLIF);
    Abc_NtkDelete(pWinNtk);
}
