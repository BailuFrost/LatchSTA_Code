#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>
#include <map>
#include <iostream>
#include <chrono>
#include <string>
#include "ell.hpp"
#include "cutting_plane.hpp"
#include "timing_graph.hpp"
#include "timing_oracle.hpp"
#include "howard.hpp"
#include "YTO.hpp"
#include "LP.hpp"
using namespace std;
using namespace chrono;
using Arr = xt::xarray<double, xt::layout_type::row_major>;
double a = 1.0;

static void printSkewSchedule(const string &tag, const vector<double> &skew)
{
     cout << "Skew schedule s_k from " << tag << " (index 0 = IO, unit ns):" << endl;
     if (skew.empty())
     {
          cout << "\t(unavailable)" << endl;
          return;
     }
     cout << "\tn_latch = " << skew.size() << endl;
     for (size_t k = 0; k < skew.size(); ++k)
     {
          cout << "\ts[" << k << "] = " << skew[k];
          if (k == kIoNodeIdx)
               cout << "  (IO)";
          cout << endl;
     }
}

static double maxAbsDiff(const vector<double> &a, const vector<double> &b)
{
     if (a.size() != b.size() || a.empty())
          return numeric_limits<double>::infinity();
     double m = 0.0;
     for (size_t i = 0; i < a.size(); ++i)
          m = max(m, fabs(a[i] - b[i]));
     return m;
}

int main(int argc, char *const argv[])
{
     auto change = high_resolution_clock::now() - high_resolution_clock::now();
     string filename(argv[1]);
     cout << filename << endl;
     int iter = atoi(argv[2]);
     double alpha = atof(argv[3]);
     Graph g_mcr;
     Graph g_ell;
     double tInitial;
     getMcrGraph(filename, g_mcr, tInitial);
     getEllGraph(filename, g_ell, tInitial);
     nanoseconds time_howard(0), time_yto(0), time_lp_mcr(0), time_ell(0), time_lp_ell(0);

     double t_lp_mcr, t_lp_ell, w_lp_ell;
     double t_howard, t_yto, t_ell, w_ell;
     McrResult how_last;
     McrResult yto_last;
     vector<double> skew_lp_raw;
     vector<double> pot_lp;
     vector<double> skew_lp_canonical;
     for (int iq = 0; iq < iter; iq++)
     {
          //solve mcr by howard
          Howard how(g_mcr, boost::num_vertices(g_mcr), tInitial, 1e-10);
          auto beginTime = high_resolution_clock::now();
          how.run();
          auto endTime = high_resolution_clock::now();
          time_howard += endTime - beginTime;
          how_last = how.getResult();
          if (how_last.status == McrStatus::Optimal)
               t_howard = how_last.period;
          else
               t_howard = numeric_limits<double>::quiet_NaN();
          //solve mcr by YTO
          YTO yto(g_mcr, boost::num_vertices(g_mcr), tInitial, 1e-10);
          beginTime = high_resolution_clock::now();
          yto.run();
          endTime = high_resolution_clock::now();
          time_yto += endTime - beginTime;
          yto_last = yto.getResult();
          if (yto_last.status == McrStatus::Optimal)
               t_yto = yto_last.period;
          else
               t_yto = numeric_limits<double>::quiet_NaN();
          //solve mcr by LP
          time_lp_mcr += mcrLP(filename, t_lp_mcr, skew_lp_raw, pot_lp);
          const double rho_lp = (tInitial - t_lp_mcr) / 2.0;
          fillMcrSkew(g_mcr, rho_lp, t_lp_mcr, 1e-10, skew_lp_canonical);
          //solve ell
          auto P = timing_oracle<Graph>(g_ell);
          auto E = ell(alpha, Arr{tInitial, 0});
          const auto options = Options{1000000, 1e-6};
          auto t = std::numeric_limits<double>::max();
          beginTime = high_resolution_clock::now();
          auto [x, ell_info] = cutting_plane_dc(P, E, t, options);
          endTime = high_resolution_clock::now();
          time_ell += endTime - beginTime;
          t_ell = x[0];
          w_ell = x[1];
          //solve ell by LP
          time_lp_ell += ellLP(filename, t_lp_ell, w_lp_ell);
     }
     cout << "=====================SUMMARY====================" << endl;
     cout << "Input file is " << filename << endl;
     cout << "Initial T is " << tInitial << " ns" << endl;
     cout << "LatchTiming defaults: Tsu=Thd=Tcq=Tdq=0 (not in benchmark)." << endl;
     cout << "MCR uses clock skew s_k with s_IO=0 and 0 <= s_k < T; ELL unchanged." << endl;
     cout << "MCR:" << endl;
     cout << "\t"
          << "#vertices = " << boost::num_vertices(g_mcr) << ", #edges = " << boost::num_edges(g_mcr) << endl;
     cout << "\t"
          << "T_Howard's = " << t_howard << " ns, time is " << time_howard.count() / (1000000. * iter)
          << " ms, status = " << mcrStatusToString(how_last.status)
          << ", cycle_len = " << how_last.criticalCycle.size()
          << ", max_violation = " << how_last.maxViolation
          << ", iters = " << how_last.iterations << endl;
     if (how_last.status == McrStatus::Optimal)
     {
          const double howard_lp_gap = fabs(t_howard - t_lp_mcr);
          cout << "\t"
               << "|T_Howard - T_LP_MCR| = " << howard_lp_gap << " ns";
          if (howard_lp_gap > 1e-4)
               cout << "  [FAIL: exceeds 1e-4 ns]";
          cout << endl;
     }
     else
     {
          cout << "\t"
               << "Howard did not return Optimal; T_Howard is not used as the clock period." << endl;
     }
     cout << "\t"
          << "T_YTO's = " << t_yto << " ns, time is " << time_yto.count() / (1000000. * iter)
          << " ms, status = " << mcrStatusToString(yto_last.status)
          << ", cycle_len = " << yto_last.criticalCycle.size()
          << ", max_violation = " << yto_last.maxViolation
          << ", iters = " << yto_last.iterations << endl;
     if (yto_last.status == McrStatus::Optimal)
     {
          const double yto_lp_gap = fabs(t_yto - t_lp_mcr);
          cout << "\t"
               << "|T_YTO - T_LP_MCR| = " << yto_lp_gap << " ns";
          if (yto_lp_gap > 1e-4)
               cout << "  [FAIL: exceeds 1e-4 ns]";
          cout << endl;
     }
     else
     {
          cout << "\t"
               << "YTO did not return Optimal; T_YTO is not used as the clock period." << endl;
     }
     cout << "\t"
          << "T_LP_MCR = " << t_lp_mcr << " ns, time is " << time_lp_mcr.count() / (1000000. * iter) << " ms" << endl;
     {
          const double rho_lp = (tInitial - t_lp_mcr) / 2.0;
          const double lp_graph_viol = mcrPotentialViolation(g_mcr, rho_lp, pot_lp, 1e-10);
          cout << "\t"
               << "LP primal mapped onto MCR graph: max reduced-cost violation = " << lp_graph_viol
               << " ns (0 means the simplex s_k is another feasible optimum at the same T)" << endl;
     }
     cout << "T* is unique; s_k at T* is not. Howard/YTO share shortest-path potentials;" << endl;
     cout << "GLPK returns a basic feasible solution that can look very different but still meet T*." << endl;
     cout << "Retarget the circuit with the canonical schedule (same extraction as Howard/YTO)." << endl;
     printSkewSchedule("canonical (graph potentials at T_LP)", skew_lp_canonical);
     printSkewSchedule("LP simplex (alternative basic solution)", skew_lp_raw);
     if (how_last.status == McrStatus::Optimal)
          printSkewSchedule("Howard", how_last.skew);
     if (yto_last.status == McrStatus::Optimal)
          printSkewSchedule("YTO", yto_last.skew);
     if (!skew_lp_canonical.empty() && how_last.skew.size() == skew_lp_canonical.size())
          cout << "max |s_Howard - s_canonical| = " << maxAbsDiff(how_last.skew, skew_lp_canonical) << " ns" << endl;
     if (how_last.skew.size() == yto_last.skew.size() && !how_last.skew.empty())
          cout << "max |s_Howard - s_YTO| = " << maxAbsDiff(how_last.skew, yto_last.skew) << " ns" << endl;
     if (!skew_lp_raw.empty() && how_last.skew.size() == skew_lp_raw.size())
          cout << "max |s_Howard - s_LP_simplex| = " << maxAbsDiff(how_last.skew, skew_lp_raw) << " ns" << endl;
     cout << "ELL:" << endl;
     cout << "\t"
          << "#vertices = " << boost::num_vertices(g_ell) << ", #edges = " << boost::num_edges(g_ell) << endl;
     cout << "\t"
          << "T_ELL = " << t_ell << " ns, W_ELL = " << w_ell << " ns, time is " << time_ell.count() / (1000000. * iter) << " ms" << endl;
     cout << "\t"
          << "T_LP_ELL = " << t_lp_ell << " ns, W_LP_ELL = " << w_lp_ell << " ns, time is " << time_lp_ell.count() / (1000000. * iter) << " ms" << endl;
     return 0;
}
