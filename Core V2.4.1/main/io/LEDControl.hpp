
enum class LEDDisplayState {
    IDLE,
    UNLOCKED,
    ALWAYS_ON,
    LOCKOUT,
    NEXT_CARD,
    STARTUP,
    WELCOMING,
    WELCOMED,
    ALWAYS_ON_WAITING,
    LOCKOUT_WAITING,
    IDLE_WAITING,
    AWAIT_AUTH,
    DENIED,
    FAULT,
    RESTART,
    IDENTIFY,
};

int led_init();

bool set_led_state(LEDDisplayState state);