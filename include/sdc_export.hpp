#pragma once
#include "mcr_result.hpp"

// Creates a new file only; v3 STA conditions and validated Howard/LP results are required.
void exportHowardSdc(const std::string &output, const DelayGraph &dg, double initial,
                     double duty, const McrResult &howard, const McrResult &lp);
