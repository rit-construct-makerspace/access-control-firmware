/*

Machine State

This task is responsible for the core logic, such as commanding light and buzzer patterns, indicating when event-based statuses should be sent, and initializing the unlock.


Global Variables Used:
LogoffSent: Flag to indicate that the status report "Session End" has been sent, so data can be cleared and reset.
State: Plaintext indication of the state of the system for status reports.
StateChange: Flag to indicate the state string is currently being updated, and not to use it.
ReadFailed: set to 1 if a card was not read properly.

*/
