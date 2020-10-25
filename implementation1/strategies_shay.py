import mean_reversion_shay 
import momentum_shay 
import random_buy_shay 
import random_sell_shay
import buy_strategy_shay 
import sell_strategy_shay 


strategies = { 
    'mean_reversion' : mean_reversion_shay , #simple mean reversion (params ma and threshold)
    'momentum' : momentum_shay, #simple momentum (params p1 and p2 represent weightings of instaneous price at 2 most recent timepoints , and threshold parameter triggers the trade)  
    'random_buy' : random_buy_shay ,  #buys with probability p 
    'random_sell' : random_sell_shay, #sells with probability p
    'buy' : buy_strategy_shay,  #buys every iteration
    'sell' : sell_strategy_shay , #sells every iteration 
}