#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>
#include "mcr_result.hpp"

using namespace std;

void generateSimpleGraph(Graph &g)
{
    g.clear();
    vector<Vertex> vertices(3);
    for (size_t i = 0; i < 3; ++i)
    {
        NodeProperties np = {i};
        vertices[i] = add_vertex(np, g);
    }
    ArcProperties ap01 = {true, 0., 6.0};
    add_edge(vertices[0], vertices[1], ap01, g);
    ArcProperties ap10 = {false, 0., 4.0};
    add_edge(vertices[1], vertices[0], ap10, g);
    ArcProperties ap_01 = {false, 0., 3.0};
    add_edge(vertices[0], vertices[1], ap_01, g);
    ArcProperties ap_10 = {true, 0., 5.0};
    add_edge(vertices[1], vertices[0], ap_10, g);
    ArcProperties ap12 = {true, 0., 4.0};
    add_edge(vertices[1], vertices[2], ap12, g);
    ArcProperties ap21 = {false, 0., 2.0};
    add_edge(vertices[2], vertices[1], ap21, g);
}

static bool readCompressedDelayGraph(const string &fileName, DelayGraph &dg,
                                     double &tInitial, bool rslt)
{
    dg.clear();
    tInitial = 0.0;
    ifstream infile(fileName);
    size_t lineNumber = 0;
    const auto fail = [&](const string &message) {
        cerr << fileName << ':' << lineNumber << ": " << message << endl;
        dg.clear();
        tInitial = 0.0;
        return false;
    };
    if (!infile)
        return fail("Cannot open input");
    map<string, size_t> indices;
    if (rslt)
        indices["IO"] = kIoNodeIdx;
    else
        indices["INPUT"] = indices["OUTPUT"] = kIoNodeIdx;
    boost::add_vertex(NodeIdx{kIoNodeIdx, {}}, dg);
    const auto node = [&](const string &name) {
        auto found = indices.find(name);
        if (found != indices.end())
            return found->second;
        const size_t idx = boost::num_vertices(dg);
        indices[name] = idx;
        boost::add_vertex(NodeIdx{idx, {}}, dg);
        return idx;
    };
    string line;
    vector<string> words;
    while (getline(infile, line))
    {
        ++lineNumber;
        splitString(line, " \t\r", words);
        if (words.empty() || words[0][0] == '#')
            continue;
        if (!rslt && (words[0] == "Network" ||
            (words.size() >= 4 && words[2] == "max_delay" && words[3] == "min_delay")))
            continue;
        if (words.size() < 4)
            return fail("Expected source, target and two delays");
        double maxDelay, minDelay;
        try
        {
            size_t usedMax = 0, usedMin = 0;
            const string &maxText = words[rslt ? 3 : 2];
            const string &minText = words[rslt ? 2 : 3];
            maxDelay = stod(maxText, &usedMax);
            minDelay = stod(minText, &usedMin);
            if (usedMax != maxText.size() || usedMin != minText.size())
                return fail("Invalid delay value");
        }
        catch (const exception &)
        {
            return fail("Invalid delay value");
        }
        if (!isfinite(maxDelay) || !isfinite(minDelay) || minDelay < 0.0 || minDelay > maxDelay)
            return fail("Delays must be finite and satisfy 0 <= min <= max");
        const string fromName = rslt && words[0].find("DFF") == string::npos ? "IO" : words[0];
        const string toName = rslt && words[1].find("DFF") == string::npos ? "IO" : words[1];
        const size_t from = node(fromName), to = node(toName);
        auto existing = boost::edge(from, to, dg);
        if (existing.second)
        {
            dg[existing.first].maxDelay = max(dg[existing.first].maxDelay, maxDelay);
            dg[existing.first].minDelay = min(dg[existing.first].minDelay, minDelay);
        }
        else
            boost::add_edge(from, to, EdgeDelay{maxDelay, minDelay}, dg);
        tInitial = max(tInitial, maxDelay);
    }
    if (infile.bad())
        return fail("Input read failed");
    if (boost::num_edges(dg) == 0)
        return fail("No timing paths in input");
    return true;
}

bool getDelayGraph(const string &fileName, DelayGraph &dg, double &tInitial)
{
    return readCompressedDelayGraph(fileName, dg, tInitial, false);
}

bool getRsltDelayGraph(const string &fileName, DelayGraph &dg, double &tInitial)
{
    return readCompressedDelayGraph(fileName, dg, tInitial, true);
}

bool readDelayGraph(const string &fileName, DelayGraph &dg, double &tInitial)
{
    const string suffix = extension(fileName);
    if (suffix == "out")
        return getDelayGraph(fileName, dg, tInitial);
    if (suffix == "rslt")
        return getRsltDelayGraph(fileName, dg, tInitial);
    dg.clear();
    tInitial = 0.0;
    cerr << "Unsupported timing input: " << fileName << endl;
    return false;
}

bool validDelayGraph(const DelayGraph &dg, double tInitial)
{
    const size_t n = boost::num_vertices(dg);
    if (n == 0 || n > numeric_limits<size_t>::max() / kMcrVarsPerNode ||
        !isfinite(tInitial) || tInitial < 0.0)
        return false;
    for (size_t k = 0; k < n; ++k)
    {
        if (dg[k].idx != k)
            return false;
        const auto &t = dg[k].timing;
        for (double value : {t.tsu, t.thd, t.tdqMin, t.tdqMax, t.tcqMin, t.tcqMax})
            if (!isfinite(value) || (k == kIoNodeIdx && value != 0.0))
                return false;
        // Setup/hold may be signed; propagation delays must be ordered and nonnegative.
        if (t.tdqMin < 0.0 || t.tdqMin > t.tdqMax || t.tcqMin < 0.0 || t.tcqMin > t.tcqMax)
            return false;
    }
    auto range = boost::edges(dg);
    for (auto e = range.first; e != range.second; ++e)
    {
        const auto &d = dg[*e];
        if (!isfinite(d.minDelay) || !isfinite(d.maxDelay) || d.minDelay < 0.0 || d.minDelay > d.maxDelay)
            return false;
    }
    return true;
}

bool validMcrGraph(const Graph &g)
{
    if (boost::num_vertices(g) == 0)
        return false;
    for (size_t k = 0; k < boost::num_vertices(g); ++k)
        if (g[k].nodeIndex != k)
            return false;
    auto range = boost::edges(g);
    for (auto e = range.first; e != range.second; ++e)
        if (!isfinite(g[*e].weight) || !isfinite(g[*e].delay))
            return false;
    return true;
}

void getMcrGraph(const string &fileName, Graph &g, double &tInitial)
{
    DelayGraph dg;
    g.clear();
    if (!readDelayGraph(fileName, dg, tInitial))
        throw runtime_error("Cannot read delay graph: " + fileName);
    getMcrGraph(dg, g, tInitial);
}

void getMcrGraph(const DelayGraph &dg, Graph &g, double tInitial)
{
    g.clear();
    if (!validDelayGraph(dg, tInitial))
        throw invalid_argument("Invalid MCR delay graph or latch timing");
    const size_t n = boost::num_vertices(dg);
    for (size_t k = 0; k < kMcrVarsPerNode * n; ++k)
        boost::add_vertex(NodeProperties{k}, g);
    // x[v] - x[u] <= c + b*T; T = T0 - 2*ratio.
    const auto constraint = [&](size_t u, size_t v, double c, double b) {
        boost::add_edge(u, v, ArcProperties{0, c + b * tInitial, 2.0 * b}, g);
    };
    const size_t io = mcrIndex(kIoNodeIdx, kMcrSkew);
    constraint(io, io, 0.0, 1.0); // T >= 0, including graphs without timing paths.
    for (size_t k = 0; k < n; ++k)
    {
        const size_t a = mcrIndex(k, kMcrA), A = mcrIndex(k, kMcrLatestA);
        const size_t d = mcrIndex(k, kMcrD), D = mcrIndex(k, kMcrLatestD);
        const size_t s = mcrIndex(k, kMcrSkew);
        const auto &t = dg[k].timing;
        if (k != kIoNodeIdx)
        {
            constraint(s, io, 0.0, 0.0);
            constraint(io, s, -kMcrSkewMargin, 1.0);
        }
        constraint(a, s, -t.thd, 0.0);
        constraint(s, A, -t.tsu, 1.0);
        constraint(A, a, 0.0, 0.0);
        constraint(D, d, 0.0, 0.0);
        constraint(d, a, -t.tdqMin, 0.0);
        constraint(D, A, -t.tdqMax, 0.0);
        constraint(d, s, -t.tcqMin, -0.5);
        constraint(D, s, -t.tcqMax, -0.5);
    }
    auto range = boost::edges(dg);
    for (auto e = range.first; e != range.second; ++e)
    {
        const size_t i = dg[boost::source(*e, dg)].idx, j = dg[boost::target(*e, dg)].idx;
        constraint(mcrIndex(i, kMcrD), mcrIndex(j, kMcrA), dg[*e].minDelay, -1.0);
        constraint(mcrIndex(j, kMcrLatestA), mcrIndex(i, kMcrLatestD), -dg[*e].maxDelay, 1.0);
    }
    if (!validMcrGraph(g))
    {
        g.clear();
        throw invalid_argument("MCR edge coefficients overflowed");
    }
}

double mcrPotentialViolation(const Graph &g, double ratio, const vector<double> &potential)
{
    if (!validMcrGraph(g) || !isfinite(ratio) || potential.size() != boost::num_vertices(g))
        return numeric_limits<double>::infinity();
    for (double x : potential)
        if (!isfinite(x))
            return numeric_limits<double>::infinity();
    double violation = 0.0;
    auto range = boost::edges(g);
    for (auto e = range.first; e != range.second; ++e)
    {
        const double v = potential[boost::target(*e, g)] - potential[boost::source(*e, g)]
                         - g[*e].weight + ratio * g[*e].delay;
        if (!isfinite(v))
            return numeric_limits<double>::infinity();
        violation = max(violation, v);
    }
    return violation;
}

double mcrScheduleViolation(const DelayGraph &dg, double period, const vector<double> &potential)
{
    if (!validDelayGraph(dg, 0.0) || !isfinite(period) || period < 0.0 ||
        potential.size() != kMcrVarsPerNode * boost::num_vertices(dg))
        return numeric_limits<double>::infinity();
    for (double x : potential)
        if (!isfinite(x))
            return numeric_limits<double>::infinity();
    double violation = fabs(potential[mcrIndex(kIoNodeIdx, kMcrSkew)]);
    const auto check = [&](double residual) {
        violation = !isfinite(residual) ? numeric_limits<double>::infinity() : max(violation, residual);
    };
    for (size_t k = 0; k < boost::num_vertices(dg); ++k)
    {
        const double a = potential[mcrIndex(k, kMcrA)], A = potential[mcrIndex(k, kMcrLatestA)];
        const double d = potential[mcrIndex(k, kMcrD)], D = potential[mcrIndex(k, kMcrLatestD)];
        const double s = potential[mcrIndex(k, kMcrSkew)];
        const auto &t = dg[k].timing;
        if (k != kIoNodeIdx)
        {
            if (s >= period)
                return numeric_limits<double>::infinity();
            check(-s);
            check(s - period + kMcrSkewMargin);
        }
        check(s + t.thd - a);
        check(A - s - period + t.tsu);
        check(a - A);
        check(d - D);
        check(a + t.tdqMin - d);
        check(A + t.tdqMax - D);
        check(s + period / 2.0 + t.tcqMin - d);
        check(s + period / 2.0 + t.tcqMax - D);
    }
    auto range = boost::edges(dg);
    for (auto e = range.first; e != range.second; ++e)
    {
        const size_t i = dg[boost::source(*e, dg)].idx, j = dg[boost::target(*e, dg)].idx;
        check(potential[mcrIndex(j, kMcrA)] + period - potential[mcrIndex(i, kMcrD)] - dg[*e].minDelay);
        check(potential[mcrIndex(i, kMcrLatestD)] + dg[*e].maxDelay - potential[mcrIndex(j, kMcrLatestA)] - period);
    }
    return violation;
}

bool fillMcrSkew(const Graph &g, double ratio, double period, double eps,
                 vector<double> &skew, vector<double> &potential)
{
    skew.clear();
    potential.clear();
    const size_t n = boost::num_vertices(g);
    if (!validMcrGraph(g) || n % kMcrVarsPerNode != 0 || !isfinite(ratio) ||
        !isfinite(period) || period < 0.0 || !isfinite(eps) || eps <= 0.0)
        return false;
    vector<double> x(n, 0.0); // A virtual zero source reaches every component.
    auto range = boost::edges(g);
    for (size_t pass = 0; pass < n; ++pass)
    {
        bool changed = false;
        for (auto e = range.first; e != range.second; ++e)
        {
            const size_t u = boost::source(*e, g), v = boost::target(*e, g);
            const double candidate = x[u] + g[*e].weight - ratio * g[*e].delay;
            if (!isfinite(candidate))
                return false;
            if (x[v] > candidate + eps)
            {
                x[v] = candidate;
                changed = true;
            }
        }
        if (!changed)
            break;
        if (pass + 1 == n)
            return false; // A negative cycle makes this candidate period infeasible.
    }
    const double origin = x[mcrIndex(kIoNodeIdx, kMcrSkew)];
    for (double &value : x)
        value -= origin;
    if (mcrPotentialViolation(g, ratio, x) > kMcrFeasibilityEps)
        return false;
    vector<double> schedule(n / kMcrVarsPerNode);
    for (size_t k = 0; k < schedule.size(); ++k)
    {
        schedule[k] = x[mcrIndex(k, kMcrSkew)];
        if (k != kIoNodeIdx && (schedule[k] < -kMcrFeasibilityEps || schedule[k] >= period))
            return false;
    }
    skew = std::move(schedule);
    potential = std::move(x);
    return true;
}

void finalizeMcrResult(const Graph &g, double tInitial, double eps, McrResult &result)
{
    if (result.status == McrStatus::Optimal)
    {
        bool valid = validMcrGraph(g) && isfinite(result.ratio) && isfinite(result.period) && result.period >= 0.0 &&
                     isfinite(tInitial) && tInitial >= 0.0 && isfinite(eps) && eps > 0.0 && !result.criticalCycle.empty();
        double weight = 0.0, transit = 0.0;
        for (size_t i = 0; valid && i < result.criticalCycle.size(); ++i)
        {
            const Edge e = result.criticalCycle[i];
            valid = boost::target(e, g) == boost::source(result.criticalCycle[(i + 1) % result.criticalCycle.size()], g);
            weight += g[e].weight;
            transit += g[e].delay;
        }
        valid = valid && isfinite(weight) && isfinite(transit) && transit > eps;
        if (valid)
        {
            const double tolerance = max(eps, 1e-12) * (1.0 + fabs(result.ratio));
            valid = fabs(weight / transit - result.ratio) <= tolerance &&
                    fabs(result.period - (tInitial - 2.0 * result.ratio)) <= kMcrFeasibilityEps;
        }
        if (!valid || !fillMcrSkew(g, result.ratio, result.period, eps, result.skew, result.potential))
            result.status = McrStatus::NumericalError;
        else
        {
            result.maxViolation = mcrPotentialViolation(g, result.ratio, result.potential);
            if (result.maxViolation > kMcrFeasibilityEps)
                result.status = McrStatus::NumericalError;
        }
    }
    if (result.status != McrStatus::Optimal)
    {
        result.ratio = result.period = numeric_limits<double>::quiet_NaN();
        result.skew.clear();
        result.potential.clear();
    }
}

void getEllGraph(const string &fileName, Graph &g, double &tInitial)
{
    DelayGraph dg;
    g.clear();
    if (!readDelayGraph(fileName, dg, tInitial))
        throw runtime_error("Cannot read delay graph: " + fileName);
    size_t nVertices = boost::num_vertices(dg);
    vector<Vertex> vertices(nVertices);
    for (size_t i = 0; i < nVertices; ++i)
    {
        NodeProperties np = {i};
        vertices[i] = boost::add_vertex(np, g);
    }
    // add edges to output graph
    boost::graph_traits<DelayGraph>::edge_iterator eidg, eienddg;
    for (tie(eidg, eienddg) = boost::edges(dg); eidg != eienddg; ++eidg)
    {
        Vertex u = vertices[boost::source(*eidg, dg)], v = vertices[boost::target(*eidg, dg)];
        ArcProperties apSetup1 = {0, tInitial - dg[*eidg].maxDelay, dg[*eidg].maxDelay};
        boost::add_edge(v, u, apSetup1, g);
        ArcProperties apSetup2 = {1, tInitial - dg[*eidg].maxDelay, dg[*eidg].maxDelay};
        boost::add_edge(v, u, apSetup2, g);
        ArcProperties apHold = {2, dg[*eidg].minDelay, dg[*eidg].minDelay};
        boost::add_edge(u, v, apHold, g);
    }
}
