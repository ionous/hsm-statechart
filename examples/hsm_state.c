/*
 * an example of using #HSM_STATE
 */
HSM_STATE( MyState,     // name of state
            HsmTopState,// parent state, or HsmTopState for no parent 
            0 );        // initial state to enter

/*
 * event handler for the above HSM_STATE declaration
 */
hsm_state MyStateEvent( hsm_machine hsm, hsm_context ctx, hsm_event evt )
{
    hsm_state ret= NULL; // return NULL if unhandled, or
                         // HsmStateHandled(), or
                         // HsmStateError(), or
                         // HsmStateFinal(), or
                         // some other state declared by #HSM_STATE
    return ret;
}