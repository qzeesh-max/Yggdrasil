#include <yggdrasil/state_machine.hpp>

using namespace yggdrasil;

struct bad_state : state_machine {
    enum class state { uninited, open };
    enum class event { do_open };  // no "typo_event"

    state bad_state{};

    [[=transition(state::open)]]
    on typo_event()  // this name doesn't exist in the event enum!
    {
        return accepted;
    }
};

using bad_fsm_t = build_state_machine_type<bad_state>;

int main() { return 0; }
