#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>
#include <boost/property_tree/json_parser.hpp>
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
    dg = DelayGraph{};
    tInitial = 0.0;
    ifstream infile(fileName);
    size_t lineNumber = 0;
    const auto fail = [&](const string &message) {
        cerr << fileName << ':' << lineNumber << ": " << message << endl;
        dg = DelayGraph{};
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
    boost::add_vertex(NodeIdx{kIoNodeIdx, {}, "IO"}, dg);
    const auto node = [&](const string &name) {
        auto found = indices.find(name);
        if (found != indices.end())
            return found->second;
        const size_t idx = boost::num_vertices(dg);
        indices[name] = idx;
        boost::add_vertex(NodeIdx{idx, {}, name}, dg);
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
    dg = DelayGraph{};
    tInitial = 0.0;
    cerr << "Unsupported timing input: " << fileName << endl;
    return false;
}

bool validDelayGraph(const DelayGraph &dg, double tInitial)
{
    const size_t n = boost::num_vertices(dg);
    const auto &io = dg[boost::graph_bundle];
    if (n == 0 || n > numeric_limits<size_t>::max() / kMcrVarsPerNode ||
        !isfinite(tInitial) || tInitial < 0.0)
        return false;
    for (size_t k = 0; k < n; ++k)
    {
        if (dg[k].idx != k)
            return false;
        const auto &t = dg[k].timing;
        const bool boundary = k == kIoNodeIdx || dg[k].kind != NodeIdx::Kind::Latch;
        if (!io.enabled && dg[k].kind != NodeIdx::Kind::Latch)
            return false;
        for (double value : {t.tsu, t.thd, t.tdqMin, t.tdqMax, t.tcqMin, t.tcqMax})
            if (!isfinite(value) || (boundary && value != 0.0))
                return false;
        // Setup/hold may be signed; propagation delays must be ordered and nonnegative.
        if (t.tdqMin < 0.0 || t.tdqMin > t.tdqMax || t.tcqMin < 0.0 || t.tcqMin > t.tcqMax)
            return false;
        const auto &checks = dg[k].clockChecks;
        if ((boundary && checks.characterized) ||
            (!checks.characterized && (checks.minPulseHigh || checks.minPulseLow)))
            return false;
        for (const auto &pulse : {checks.minPulseHigh, checks.minPulseLow})
            if (pulse && (!isfinite(*pulse) || *pulse < 0.0))
                return false;
    }
    auto range = boost::edges(dg);
    for (auto e = range.first; e != range.second; ++e)
    {
        const auto &d = dg[*e];
        if (!isfinite(d.minDelay) || !isfinite(d.maxDelay) || d.minDelay < 0.0 || d.minDelay > d.maxDelay)
            return false;
    }
    if (io.enabled)
    {
        if (io.inputPort.empty() || io.outputPort.empty() || io.inputPort == io.outputPort ||
            io.inputMin > io.inputMax || io.outputMin > io.outputMax ||
            io.setupUncertainty < 0.0 || io.holdUncertainty < 0.0 ||
            !isfinite(io.dutyCycle) || io.dutyCycle <= 0.0 || io.dutyCycle > 0.5 || boost::degree(0, dg) != 0)
            return false;
        for (double value : {io.inputMin, io.inputMax, io.outputMin, io.outputMax,
                             io.setupUncertainty, io.holdUncertainty})
            if (!isfinite(value))
                return false;
        size_t inputs = 0, outputs = 0;
        vector<size_t> incoming(n);
        queue<size_t> ready;
        for (size_t k = 0; k < n; ++k)
        {
            incoming[k] = boost::in_degree(k, dg);
            if (incoming[k] == 0)
                ready.push(k);
            if (k == 0)
                continue;
            if (dg[k].kind == NodeIdx::Kind::Input)
            {
                ++inputs;
                if (dg[k].name != io.inputPort || incoming[k] || !boost::out_degree(k, dg))
                    return false;
            }
            else if (dg[k].kind == NodeIdx::Kind::Output)
            {
                ++outputs;
                if (dg[k].name != io.outputPort || !incoming[k] || boost::out_degree(k, dg))
                    return false;
            }
            else if (!incoming[k] || !boost::out_degree(k, dg))
                return false;
        }
        size_t visited = 0;
        while (!ready.empty())
        {
            const size_t k = ready.front(); ready.pop(); ++visited;
            auto edges = boost::out_edges(k, dg);
            for (auto e = edges.first; e != edges.second; ++e)
            {
                const size_t target = boost::target(*e, dg);
                if (--incoming[target] == 0)
                    ready.push(target);
            }
        }
        if (inputs != 1 || outputs != 1 || visited != n)
            return false; // Same-transaction IO mode does not support feedback/cycle reassignment.
    }
    return true;
}

double mcrPulsePeriodMin(const DelayGraph &dg, double dutyCycle)
{
    if (!validDelayGraph(dg, 0.0) || !isfinite(dutyCycle) || dutyCycle <= 0.0 || dutyCycle > 0.5)
        return numeric_limits<double>::infinity();
    double minimum = 0.0;
    for (size_t k = 1; k < boost::num_vertices(dg); ++k)
    {
        const auto &checks = dg[k].clockChecks;
        if (checks.minPulseHigh)
            minimum = max(minimum, *checks.minPulseHigh / dutyCycle);
        if (checks.minPulseLow)
            minimum = max(minimum, *checks.minPulseLow / (1.0 - dutyCycle));
    }
    return minimum;
}

void readLatchTiming(const string &fileName, DelayGraph &dg)
{
    using boost::property_tree::ptree;
    ptree root;
    boost::property_tree::read_json(fileName, root);
    const int schema = root.get<int>("schema_version");
    if ((schema < 1 || schema > 3) || root.get<string>("units.time") != "ns")
        throw invalid_argument("Latch timing requires schema_version 1, 2 or 3 and time unit ns");
    if (!validDelayGraph(dg, 0.0))
        throw invalid_argument("Invalid delay graph for latch timing");

    DelayGraph candidate = dg;
    if (schema < 3 && (candidate[boost::graph_bundle].enabled || root.get_child_optional("io_constraints")))
        throw invalid_argument("SDC IO constraints require schema_version 3; reload compressed input for legacy mode");
    map<string, size_t> remaining;
    for (size_t k = 0; k < boost::num_vertices(candidate); ++k)
        if (k != kIoNodeIdx && candidate[k].kind == NodeIdx::Kind::Latch && (candidate[k].name.empty() ||
            !remaining.emplace(candidate[k].name, k).second))
            throw invalid_argument("Latch nodes require unique instance names");

    const auto &nodes = root.get_child("nodes");
    if (!nodes.data().empty())
        throw invalid_argument("Latch timing nodes must be an array");
    for (const auto &entry : nodes)
    {
        if (!entry.first.empty())
            throw invalid_argument("Latch timing nodes must be an array");
        const string name = entry.second.get<string>("instance");
        const auto found = remaining.find(name);
        if (found == remaining.end())
            throw invalid_argument("Unknown or duplicate latch instance: " + name);
        const auto &p = entry.second.get_child("parameters_ns");
        candidate[found->second].timing = LatchTiming{
            p.get<double>("tsu"), p.get<double>("thd"),
            p.get<double>("tdqMin"), p.get<double>("tdqMax"),
            p.get<double>("tcqMin"), p.get<double>("tcqMax")};
        auto &checks = candidate[found->second].clockChecks;
        checks = {};
        if (schema >= 2)
        {
            const auto &input = entry.second.get_child("clock_checks_ns");
            if (!input.data().empty() || input.size() != 2 ||
                input.count("min_pulse_width_high") != 1 || input.count("min_pulse_width_low") != 1)
                throw invalid_argument("Clock checks require explicit high/low values or null");
            checks.characterized = true;
            const auto pulse = [&](const string &key) -> std::optional<double> {
                const auto &value = input.get_child(key);
                if (!value.empty())
                    throw invalid_argument("Pulse width must be a number or null");
                if (value.data() == "null")
                    return std::nullopt;
                return value.get_value<double>();
            };
            checks.minPulseHigh = pulse("min_pulse_width_high");
            checks.minPulseLow = pulse("min_pulse_width_low");
        }
        else if (entry.second.get_child_optional("clock_checks_ns"))
            throw invalid_argument("Clock checks require schema_version 2");
        remaining.erase(found);
    }
    if (!remaining.empty())
        throw invalid_argument("Missing latch timing for instance: " + remaining.begin()->first);
    if (schema == 3)
    {
        const auto &input = root.get_child("io_constraints");
        if (input.size() != 9 || input.get_child("input").size() != 3 || input.get_child("output").size() != 3)
            throw invalid_argument("Unexpected or incomplete SDC IO fields");
        if (input.get<string>("model") != "single_clock_rise_dag_v1" ||
            input.get<string>("clock") != root.get<string>("clock.name") ||
            input.get<int>("setup_capture_cycles") != 1 || input.get<int>("hold_capture_cycles") != 0 ||
            input.get<int>("path_cycle_shift") != 0)
            throw invalid_argument("Unsupported SDC IO model, clock or cycle mapping");
        const double period = root.get<double>("clock.period_ns");
        const double rise = root.get<double>("clock.rise_ns"), fall = root.get<double>("clock.fall_ns");
        if (!isfinite(period) || !isfinite(rise) || !isfinite(fall) || period <= 0.0 ||
            fabs(fall - period) > 1e-9 || rise / period < 0.5 || rise >= period)
            throw invalid_argument("SDC IO requires rise in [T/2,T) and fall at T");
        IoConstraints io;
        io.enabled = true;
        io.inputPort = input.get<string>("input.port");
        io.outputPort = input.get<string>("output.port");
        io.inputMin = input.get<double>("input.min_delay_ns");
        io.inputMax = input.get<double>("input.max_delay_ns");
        io.outputMin = input.get<double>("output.min_delay_ns");
        io.outputMax = input.get<double>("output.max_delay_ns");
        io.setupUncertainty = input.get<double>("setup_uncertainty_ns");
        io.holdUncertainty = input.get<double>("hold_uncertainty_ns");
        io.dutyCycle = (fall - rise) / period;
        if (const auto conditions = root.get_child_optional("sta_conditions"))
        {
            if (conditions->size() != 4 || root.get<string>("units.capacitance") != "pF")
                throw invalid_argument("Unexpected or incomplete STA operating conditions");
            io.staConditions = IoConstraints::StaConditions{input.get<string>("clock"),
                conditions->get<string>("clock_port"), conditions->get<double>("clock_transition_ns"),
                conditions->get<double>("input_transition_ns"), conditions->get<double>("output_load_pf")};
            const auto &sta = *io.staConditions;
            if (sta.clockPort.empty() || sta.clockPort == io.inputPort || sta.clockPort == io.outputPort)
                throw invalid_argument("Invalid STA clock port");
            for (double value : {sta.clockTransition, sta.inputTransition, sta.outputLoad})
                if (!isfinite(value) || value < 0.0)
                    throw invalid_argument("Invalid STA operating condition");
            for (const auto &node : root.get_child("nodes"))
            {
                const double slew = node.second.get<double>("clock_checks_conditions.clock_slew_ns");
                if (!isfinite(slew) || fabs(slew - sta.clockTransition) > 1e-6)
                    throw invalid_argument("STA clock transition does not match pulse-width characterization");
            }
        }
        map<string, size_t> endpoints;
        for (size_t k = 1; k < boost::num_vertices(candidate); ++k)
            endpoints.emplace(candidate[k].name, k);
        endpoints["INPUT"] = candidate[boost::graph_bundle].enabled ? endpoints.at(io.inputPort) : 0;
        endpoints["OUTPUT"] = candidate[boost::graph_bundle].enabled ? endpoints.at(io.outputPort) : 0;
        const auto &paths = root.get_child("edges");
        if (paths.size() != boost::num_edges(candidate))
            throw invalid_argument("JSON paths do not match the compressed delay graph");
        set<pair<size_t, size_t>> seen;
        for (const auto &entry : paths)
        {
            const auto &path = entry.second;
            const string source = path.get<string>("source"), target = path.get<string>("target");
            if (source == "OUTPUT" || target == "INPUT" || !endpoints.count(source) || !endpoints.count(target))
                throw invalid_argument("Invalid JSON IO path endpoints");
            const size_t u = endpoints.at(source), v = endpoints.at(target);
            const auto edge = boost::edge(u, v, candidate);
            const double lo = path.get<double>("min_delay_ns"), hi = path.get<double>("max_delay_ns");
            if (!edge.second || !seen.emplace(u, v).second || !isfinite(lo) || !isfinite(hi) ||
                fabs(candidate[edge.first].minDelay - lo) > 1e-12 || fabs(candidate[edge.first].maxDelay - hi) > 1e-12)
                throw invalid_argument("JSON path delays do not match the compressed delay graph");
        }
        if (!candidate[boost::graph_bundle].enabled)
        {
            DelayGraph separate;
            for (size_t k = 0; k < boost::num_vertices(candidate); ++k)
            {
                if (candidate[k].name == io.inputPort || candidate[k].name == io.outputPort)
                    throw invalid_argument("SDC port name collides with a latch or anchor");
                boost::add_vertex(candidate[k], separate);
            }
            const size_t inputIdx = boost::num_vertices(separate), outputIdx = inputIdx + 1;
            NodeIdx portIn{inputIdx, {}, io.inputPort}, portOut{outputIdx, {}, io.outputPort};
            portIn.kind = NodeIdx::Kind::Input; portOut.kind = NodeIdx::Kind::Output;
            boost::add_vertex(portIn, separate); boost::add_vertex(portOut, separate);
            auto edges = boost::edges(candidate);
            for (auto e = edges.first; e != edges.second; ++e)
            {
                const size_t source = boost::source(*e, candidate), target = boost::target(*e, candidate);
                boost::add_edge(source == 0 ? inputIdx : source, target == 0 ? outputIdx : target,
                                candidate[*e], separate);
            }
            candidate = std::move(separate);
        }
        candidate[boost::graph_bundle] = io;
    }
    if (!validDelayGraph(candidate, 0.0))
        throw invalid_argument("Latch timing must be finite with 0 <= propagation min <= max and nonnegative pulse widths");
    dg = std::move(candidate);
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

void getMcrGraph(const string &fileName, Graph &g, double &tInitial, double dutyCycle)
{
    DelayGraph dg;
    g.clear();
    if (!readDelayGraph(fileName, dg, tInitial))
        throw runtime_error("Cannot read delay graph: " + fileName);
    getMcrGraph(dg, g, tInitial, dutyCycle);
}

void getMcrGraph(const DelayGraph &dg, Graph &g, double tInitial, double dutyCycle)
{
    g.clear();
    if (!validDelayGraph(dg, tInitial))
        throw invalid_argument("Invalid MCR delay graph or latch timing");
    if (!isfinite(dutyCycle) || dutyCycle <= 0.0 || dutyCycle > 0.5)
        throw invalid_argument("MCR duty cycle must be finite and in (0, 0.5]");
    const double pulseMinimum = mcrPulsePeriodMin(dg, dutyCycle);
    if (!isfinite(pulseMinimum))
        throw invalid_argument("MCR pulse-width period lower bound overflowed");
    const size_t n = boost::num_vertices(dg);
    const auto &boundary = dg[boost::graph_bundle];
    for (size_t k = 0; k < kMcrVarsPerNode * n; ++k)
        boost::add_vertex(NodeProperties{k}, g);
    // x[v] - x[u] <= c + b*T; T = T0 - 2*ratio.
    const auto constraint = [&](size_t u, size_t v, double c, double b) {
        boost::add_edge(u, v, ArcProperties{0, c + b * tInitial, 2.0 * b}, g);
    };
    const auto equal = [&](size_t u, size_t v, double c, double b) {
        constraint(u, v, c, b);
        constraint(v, u, -c, -b);
    };
    const size_t io = mcrIndex(kIoNodeIdx, kMcrSkew);
    constraint(io, io, -pulseMinimum, 1.0); // T >= max(high/duty, low/(1-duty)), or zero for legacy inputs.
    for (size_t k = 0; k < n; ++k)
    {
        const size_t a = mcrIndex(k, kMcrA), A = mcrIndex(k, kMcrLatestA);
        const size_t d = mcrIndex(k, kMcrD), D = mcrIndex(k, kMcrLatestD);
        const size_t s = mcrIndex(k, kMcrSkew);
        const auto &t = dg[k].timing;
        if (boundary.enabled && (k == kIoNodeIdx || dg[k].kind != NodeIdx::Kind::Latch))
        {
            if (k == kIoNodeIdx)
                for (const auto variable : {kMcrA, kMcrLatestA, kMcrD, kMcrLatestD})
                    equal(io, mcrIndex(k, variable), 0.0, 0.0);
            else
            {
                equal(io, s, 0.0, 0.0);
                if (dg[k].kind == NodeIdx::Kind::Input)
                {
                    equal(io, a, boundary.inputMin, 1.0 - dutyCycle);
                    equal(io, d, boundary.inputMin, 1.0 - dutyCycle);
                    equal(io, A, boundary.inputMax, 1.0 - dutyCycle);
                    equal(io, D, boundary.inputMax, 1.0 - dutyCycle);
                }
                else
                {
                    equal(a, d, 0.0, 0.0); equal(A, D, 0.0, 0.0);
                    constraint(A, a, 0.0, 0.0);
                    constraint(a, io, boundary.outputMin - boundary.holdUncertainty, -(1.0 - dutyCycle));
                    constraint(io, A, -boundary.outputMax - boundary.setupUncertainty, 2.0 - dutyCycle);
                }
            }
            continue;
        }
        if (k != kIoNodeIdx)
        {
            constraint(s, io, 0.0, 0.0);
            constraint(io, s, -kMcrSkewMargin, 1.0);
        }
        constraint(a, s, -t.thd - boundary.holdUncertainty, 0.0);
        constraint(s, A, -t.tsu - boundary.setupUncertainty, 1.0);
        constraint(A, a, 0.0, 0.0);
        constraint(D, d, 0.0, 0.0);
        constraint(d, a, -t.tdqMin, 0.0);
        constraint(D, A, -t.tdqMax, 0.0);
        constraint(d, s, -t.tcqMin, -(1.0 - dutyCycle));
        constraint(D, s, -t.tcqMax, -(1.0 - dutyCycle));
    }
    auto range = boost::edges(dg);
    for (auto e = range.first; e != range.second; ++e)
    {
        const size_t i = dg[boost::source(*e, dg)].idx, j = dg[boost::target(*e, dg)].idx;
        const double shift = boundary.enabled ? 0.0 : 1.0;
        constraint(mcrIndex(i, kMcrD), mcrIndex(j, kMcrA), dg[*e].minDelay, -shift);
        constraint(mcrIndex(j, kMcrLatestA), mcrIndex(i, kMcrLatestD), -dg[*e].maxDelay, shift);
        if (boundary.enabled && dg[i].kind == NodeIdx::Kind::Latch)
        {
            // shortcut: hold uses an opening-response lower bound; use exact min propagation when less conservative schedules are needed.
            const double earliest = min(dg[i].timing.tcqMin, dg[i].timing.tdqMin) + dg[*e].minDelay;
            if (dg[j].kind == NodeIdx::Kind::Output)
                constraint(mcrIndex(i, kMcrSkew), io, earliest + boundary.outputMin - boundary.holdUncertainty, 0.0);
            else
                constraint(mcrIndex(i, kMcrSkew), mcrIndex(j, kMcrSkew),
                           earliest - dg[j].timing.thd - boundary.holdUncertainty, 1.0 - dutyCycle);
        }
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

double mcrScheduleViolation(const DelayGraph &dg, double period, const vector<double> &potential,
                            double dutyCycle)
{
    if (!validDelayGraph(dg, 0.0) || !isfinite(period) || period < 0.0 ||
        !isfinite(dutyCycle) || dutyCycle <= 0.0 || dutyCycle > 0.5 ||
        potential.size() != kMcrVarsPerNode * boost::num_vertices(dg))
        return numeric_limits<double>::infinity();
    for (double x : potential)
        if (!isfinite(x))
            return numeric_limits<double>::infinity();
    const double pulseMinimum = mcrPulsePeriodMin(dg, dutyCycle);
    if (!isfinite(pulseMinimum))
        return numeric_limits<double>::infinity();
    double violation = max(fabs(potential[mcrIndex(kIoNodeIdx, kMcrSkew)]), pulseMinimum - period);
    const auto check = [&](double residual) {
        violation = !isfinite(residual) ? numeric_limits<double>::infinity() : max(violation, residual);
    };
    const auto &boundary = dg[boost::graph_bundle];
    for (size_t k = 0; k < boost::num_vertices(dg); ++k)
    {
        const double a = potential[mcrIndex(k, kMcrA)], A = potential[mcrIndex(k, kMcrLatestA)];
        const double d = potential[mcrIndex(k, kMcrD)], D = potential[mcrIndex(k, kMcrLatestD)];
        const double s = potential[mcrIndex(k, kMcrSkew)];
        const auto &t = dg[k].timing;
        if (boundary.enabled && (k == kIoNodeIdx || dg[k].kind != NodeIdx::Kind::Latch))
        {
            check(fabs(s));
            if (k == kIoNodeIdx)
            {
                for (double value : {a, A, d, D}) check(fabs(value));
            }
            else if (dg[k].kind == NodeIdx::Kind::Input)
            {
                const double early = (1.0 - dutyCycle) * period + boundary.inputMin;
                const double late = (1.0 - dutyCycle) * period + boundary.inputMax;
                for (double value : {a, d}) check(fabs(value - early));
                for (double value : {A, D}) check(fabs(value - late));
            }
            else
            {
                check(fabs(d - a)); check(fabs(D - A)); check(a - A);
                check((1.0 - dutyCycle) * period - boundary.outputMin + boundary.holdUncertainty - a);
                check(A - (2.0 - dutyCycle) * period + boundary.outputMax + boundary.setupUncertainty);
            }
            continue;
        }
        if (k != kIoNodeIdx)
        {
            if (s >= period)
                return numeric_limits<double>::infinity();
            check(-s);
            check(s - period + kMcrSkewMargin);
        }
        check(s + t.thd + boundary.holdUncertainty - a);
        check(A - s - period + t.tsu + boundary.setupUncertainty);
        check(a - A);
        check(d - D);
        check(a + t.tdqMin - d);
        check(A + t.tdqMax - D);
        check(s + (1.0 - dutyCycle) * period + t.tcqMin - d);
        check(s + (1.0 - dutyCycle) * period + t.tcqMax - D);
    }
    auto range = boost::edges(dg);
    for (auto e = range.first; e != range.second; ++e)
    {
        const size_t i = dg[boost::source(*e, dg)].idx, j = dg[boost::target(*e, dg)].idx;
        const double shift = boundary.enabled ? 0.0 : period;
        check(potential[mcrIndex(j, kMcrA)] + shift - potential[mcrIndex(i, kMcrD)] - dg[*e].minDelay);
        check(potential[mcrIndex(i, kMcrLatestD)] + dg[*e].maxDelay - potential[mcrIndex(j, kMcrLatestA)] - shift);
        if (boundary.enabled && dg[i].kind == NodeIdx::Kind::Latch)
        {
            const double early = potential[mcrIndex(i, kMcrSkew)] + (1.0 - dutyCycle) * period +
                                 min(dg[i].timing.tcqMin, dg[i].timing.tdqMin) + dg[*e].minDelay;
            const double required = dg[j].kind == NodeIdx::Kind::Output
                ? (1.0 - dutyCycle) * period - boundary.outputMin + boundary.holdUncertainty
                : potential[mcrIndex(j, kMcrSkew)] + dg[j].timing.thd + boundary.holdUncertainty;
            check(required - early);
        }
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
