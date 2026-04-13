#ifdef NS3_MODULE_COMPILATION 
    error "Do not include ns3 module aggregator headers from other modules these are meant only for end user scripts." 
#endif 
#ifndef NS3_MODULE_FIRST_ORDER_BUILDINGS_AWARE_PATH_LOSS
    // Module headers: 
    #include <ns3/first-order-buildings-aware-propagation-loss-model.h>
    #include <ns3/foba-toolbox.h>
#endif 