#pragma once
#include <boost/graph/adjacency_list.hpp>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include "strOperation.hpp"

using namespace std;
struct LatchTiming
{
    double tsu = 0.0;
    double thd = 0.0;
    double tdqMin = 0.0;
    double tdqMax = 0.0;
    double tcqMin = 0.0;
    double tcqMax = 0.0;
};

struct NodeIdx
{
    size_t idx;
    LatchTiming timing;
    string name;
    struct ClockChecks
    {
        bool characterized = false;
        std::optional<double> minPulseHigh;
        std::optional<double> minPulseLow;
    } clockChecks{};
    enum class Kind { Latch, Input, Output } kind = Kind::Latch;
};

struct IoConstraints
{
    struct StaConditions
    {
        string clockName, clockPort;
        double clockTransition, inputTransition, outputLoad;
    };
    bool enabled = false;
    string inputPort, outputPort;
    double inputMin = 0.0, inputMax = 0.0;
    double outputMin = 0.0, outputMax = 0.0;
    double setupUncertainty = 0.0, holdUncertainty = 0.0;
    double dutyCycle = 0.5;
    std::optional<StaConditions> staConditions;
};

constexpr size_t kIoNodeIdx = 0;
enum McrVariable : size_t { kMcrA, kMcrLatestA, kMcrD, kMcrLatestD, kMcrSkew, kMcrVarsPerNode };
// shortcut: s_k < T uses a fixed ns margin, revisit when timing scales approach that margin.
constexpr double kMcrSkewMargin = 1e-8;
constexpr double kMcrFeasibilityEps = 1e-7;
constexpr double kMcrPeriodEps = 1e-4;
static_assert(kMcrVarsPerNode == 5 && kMcrSkewMargin > 0.0, "Invalid MCR layout or phase margin");

inline size_t mcrIndex(size_t node, McrVariable variable)
{
    return kMcrVarsPerNode * node + variable;
}
struct EdgeDelay
{
    double maxDelay;
    double minDelay;
};

struct NodeProperties
{
    size_t nodeIndex; /**< node index, range: [0, num_vertices-1] */
};

/**
 * Definitions for Arc Properties.
 *
 */
struct ArcProperties
{
    int mode;      /*the flag of different constraint edge*/
    double weight; /* weight */
    double delay;  /*max delay of setup edge or min delay of hold edge in ell, transit time in mcr*/
};

using Graph = boost::adjacency_list<boost::multisetS, boost::vecS, boost::bidirectionalS, NodeProperties,
                                    ArcProperties>;
using DelayGraph = boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS, NodeIdx,
                                         EdgeDelay, IoConstraints>; // store the delay information (max & min) between two
                                                     // flip-flops
using Vertex = boost::graph_traits<Graph>::vertex_descriptor;
using Edge = boost::graph_traits<Graph>::edge_descriptor;
using InEdgeIterator = boost::graph_traits<Graph>::in_edge_iterator;
using OutEdgeIterator = boost::graph_traits<Graph>::out_edge_iterator;
using VertexIterator = boost::graph_traits<Graph>::vertex_iterator;
using EdgeIterator = boost::graph_traits<Graph>::edge_iterator;
//use graph to imitate tree
using ShortestPathTree = boost::adjacency_list<boost::listS, boost::vecS, boost::bidirectionalS>;
using SPTVertex = boost::graph_traits<ShortestPathTree>::vertex_descriptor;
using SPTEdge = boost::graph_traits<ShortestPathTree>::edge_descriptor;

void generateSimpleGraph(Graph &g);
/**
 * Read blif out file into a delay graph
 *
 * @param fileName
 * @param string
 * @param nodeIdxMap
 * @param g
 * @param tInitial
 *
 * @return
 */
bool getDelayGraph(const string &fileName, DelayGraph &dg, double &tInitial);
/**
 * @brief Get the Delay Graph from rslt file
 * 
 * @param fileName 
 * @param g 
 * @param tInitial 
 * @return true 
 * @return false 
 */
bool getRsltDelayGraph(const string &fileName, DelayGraph &dg, double &tInitial);
bool readDelayGraph(const string &fileName, DelayGraph &dg, double &tInitial);
// v1: six parameters; v2: pulse checks; v3: separate SDC IO on a same-transaction DAG.
void readLatchTiming(const string &fileName, DelayGraph &dg);
bool validDelayGraph(const DelayGraph &dg, double tInitial);
// Infinity denotes invalid data, duty cycle or an overflowing pulse-width lower bound.
double mcrPulsePeriodMin(const DelayGraph &dg, double dutyCycle);
bool validMcrGraph(const Graph &g);
/**
 * @brief Generate constraint graph for MCR
 * 
 * @param g 
 * @param tInitial 
 */
// dutyCycle is a fixed high-level fraction in (0, 0.5]; the window opens at s+(1-dutyCycle)*T.
void getMcrGraph(const string &fileName, Graph &g, double &tInitial, double dutyCycle = 0.5);
void getMcrGraph(const DelayGraph &dg, Graph &g, double tInitial, double dutyCycle = 0.5);
// Potentials use x[v] - x[u] <= weight - ratio * transit on u -> v.
double mcrPotentialViolation(const Graph &g, double ratio, const vector<double> &potential);
double mcrScheduleViolation(const DelayGraph &dg, double period, const vector<double> &potential,
                            double dutyCycle = 0.5);
bool fillMcrSkew(const Graph &g, double ratio, double period, double eps,
                 vector<double> &skew, vector<double> &potential);
/**
 * @brief Generate constraint graph for ellipsoid
 * 
 * @param g 
 * @param tInitial 
 */
void getEllGraph(const string &fileName, Graph &g, double &tInitial);
