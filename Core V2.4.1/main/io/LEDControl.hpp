namespace LED {
    enum class DisplayState {
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

    int init();
    bool set_state(LED::DisplayState state);
};
