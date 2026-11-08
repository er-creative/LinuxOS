#pragma once
// 5C-2 compatibility layer: new code should include cete/supervision/DevinderjeetSupervisor.hpp
// and use cete::supervision. Existing DevAI consumers remain source-compatible.
#include "cete/supervision/DevinderjeetSupervisor.hpp"

namespace devai::strategy {
using cete::supervision::DevSupervisedAction;
using cete::supervision::DevOrderStatus;
using cete::supervision::DevSupervisorConfig;
using cete::supervision::DevManagerRegistration;
using cete::supervision::DevSupervisedOrder;
using cete::supervision::DevOrderReport;
using cete::supervision::DevSupervisedPosition;
using cete::supervision::DevManagerAccount;
using cete::supervision::DevSupervisorSnapshot;
using cete::supervision::DevinderjeetSupervisor;
} // namespace devai::strategy
