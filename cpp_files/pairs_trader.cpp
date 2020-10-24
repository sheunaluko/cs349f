#include <algorithm>
#include <mutex>

#include "trader/trader_api.h"

#define HIGHEST_SELL_PRICE 99999999
#define LOWEST_BUY_PRICE 0

// Google Command Flags
/* Setup and identity flags */

DEFINE_string(configuration_path, "/root/vm_config.json",
              "Read your configuration file from this path");
DEFINE_int32(base_shares, 5000, "The base shares for pairs traders");
DEFINE_int32(moving_window, 5,
             "The window length in seconds (for pairs trading)");
DEFINE_int32(tick_length, 1,
             "The basic time unit for moving window (seconds), that means, "
             "after how much time should we record one point of stock price");
DEFINE_double(threshold, 5, "The threshold (for pairs trading)");

// Continuous Execution Indicator
static volatile bool run = true;

// Catch CTRL-C Exception and Stop Execution
void SignalHandler(int signal) { run = false; }
// Get symbols list
std::vector<std::string> symbol_list;

double GetHighestBuyPrice(std::vector<LimitOrderBook> *lob) {
  // Only Consider the highest buy price
  double highest_buy_price = LOWEST_BUY_PRICE;
  for (size_t i = 0; i < 1; i++) {
    if (i >= lob->size()) {
      return highest_buy_price;
    }
    for (auto p : (*lob)[i].buy_queue_) {
      if (highest_buy_price < (p.second).limit_price_) {
        highest_buy_price = (p.second).limit_price_;
      }
    }
  }

  return highest_buy_price;
}

double GetLowestSellPrice(std::vector<LimitOrderBook> *lob) {
  // Only Consider the lowest sell price
  double lowest_sell_price = HIGHEST_SELL_PRICE;
  for (size_t i = 0; i < 1; i++) {
    if (i >= lob->size()) {
      return lowest_sell_price;
    }
    for (auto p : (*lob)[i].sell_queue_) {
      if (lowest_sell_price > (p.second).limit_price_) {
        lowest_sell_price = (p.second).limit_price_;
      }
    }
  }
  return lowest_sell_price;
}

void PairsTradeFunc(Trader *trader_api, std::string target_symbol,
                    std::string baseline_symbol, uint32_t moving_window_size,
                    int32_t tick_length, double threshold, int base_shares) {
  std::vector<LimitOrderBook> target_recent_lobs;
  std::vector<LimitOrderBook> baseline_recent_lobs;
  uint64_t start_timestamp = utils::GetMicrosecondTimestamp();
  uint64_t target_last_seen_timestamp = 1;
  uint64_t baseline_last_seen_timestamp = 1;
  std::queue<double> target_stock_prices;
  std::queue<double> baseline_stock_prices;
  double target_latest_stock_price = 1;
  double baseline_latest_stock_price = 1;
  std::queue<Order> previous_orders;
  while (run) {
    VLOG(1) << "start_timestamp = " << start_timestamp
            << " current_time = " << utils::GetMicrosecondTimestamp()
            << " tick_length = " << tick_length;
    while (utils::GetMicrosecondTimestamp() <
           start_timestamp + tick_length * 1000 * 1000) {
      usleep(100);
    }
    start_timestamp = utils::GetMicrosecondTimestamp();
    VLOG(1) << "New Loop StartTimestamp = " << start_timestamp;
    // The top one is the most recent one
    trader_api->GetRecentLOBs(target_symbol, &target_recent_lobs,
                              target_last_seen_timestamp);
    trader_api->GetRecentLOBs(baseline_symbol, &baseline_recent_lobs,
                              baseline_last_seen_timestamp);

    if (target_recent_lobs.size() > 0 && baseline_recent_lobs.size() > 0) {
      target_last_seen_timestamp = target_recent_lobs[0].creation_timestamp_;
      // Take the highest buy price in the most recent lob as the stock price
      double target_highest_buy_price = GetHighestBuyPrice(&target_recent_lobs);
      double target_lowest_sell_price = GetLowestSellPrice(&target_recent_lobs);
      double target_current_stock_price = target_highest_buy_price;
      double baseline_highest_buy_price =
          GetHighestBuyPrice(&baseline_recent_lobs);
      double baseline_current_stock_price = baseline_highest_buy_price;

      VLOG(1) << "target_highest_buy_price=" << target_highest_buy_price << "\t"
              << "target_lowest_sell_price=" << target_lowest_sell_price << "\t"
              << "baseline_highest_buy_price=" << baseline_highest_buy_price
              << std::endl;
      if (target_current_stock_price > LOWEST_BUY_PRICE) {
        if (target_stock_prices.size() >= moving_window_size &&
            baseline_stock_prices.size() >= moving_window_size) {
          // Pairs Trading Strategy
          std::queue<double> target_stock_prices_replica = target_stock_prices;
          std::queue<double> baseline_stock_prices_replica =
              baseline_stock_prices;
          double sum_diff_price = 0;
          for (int i = 0; i < moving_window_size; i++) {
            sum_diff_price += (target_stock_prices_replica.front() -
                               baseline_stock_prices_replica.front());
          }
          double average_diff_price = sum_diff_price / moving_window_size;
          double buy_cutoff = (1.0 - threshold / 100.0) * average_diff_price;
          double sell_cutoff = (1.0 + threshold / 100.0) * average_diff_price;
          VLOG(1) << "average_diff_price= " << average_diff_price
                  << "\t buy_cutoff=" << buy_cutoff
                  << "\tsell_cutoff=" << sell_cutoff
                  << "\ttarget_current_stock_price="
                  << target_current_stock_price;
          if (target_current_stock_price <= buy_cutoff) {
            // Place a buy order for target symbol
            int num_shares = static_cast<int>(
                buy_cutoff / target_current_stock_price * base_shares);
            if (num_shares < 0) {
              num_shares = base_shares;
            }
            Order ord;
            // If I really want to buy, I should buy higher than anyone else
            trader_api->SubmitOrder(target_symbol, &ord, OrderType::limit,
                                    OrderAction::buy, num_shares,
                                    target_highest_buy_price + 1);
            LOG(ERROR) << "Submitted Buying Order " << ord.SerializeOrder();
            previous_orders.push(ord);
          } else if (target_current_stock_price >= sell_cutoff &&
                     target_lowest_sell_price < HIGHEST_SELL_PRICE) {
            // Place a sell order for target symbol
            int num_shares = static_cast<int>(target_current_stock_price /
                                              sell_cutoff * base_shares);
            if (num_shares < 0) {
              num_shares = base_shares;
            }
            Order ord;
            // If I really want to sell, I should sell lower than anyone
            trader_api->SubmitOrder(target_symbol, &ord, OrderType::limit,
                                    OrderAction::sell, num_shares,
                                    target_lowest_sell_price - 1);
            LOG(ERROR) << "Submitted Selling Order " << ord.SerializeOrder();
            previous_orders.push(ord);
          }
        }
      }
      // Update history records
      if (target_current_stock_price > 0) {
        target_stock_prices.push(target_current_stock_price);
        while (target_stock_prices.size() > moving_window_size) {
          target_stock_prices.pop();
        }
        target_latest_stock_price = target_current_stock_price;
      } else {
        // currently the lob is empty, so use the latest stock price (>0) as
        // the current stock price
        target_stock_prices.push(target_latest_stock_price);
        while (target_stock_prices.size() > moving_window_size) {
          target_stock_prices.pop();
        }
      }

      if (baseline_current_stock_price > 0) {
        baseline_stock_prices.push(baseline_current_stock_price);
        while (baseline_stock_prices.size() > moving_window_size) {
          baseline_stock_prices.pop();
        }
        baseline_latest_stock_price = baseline_current_stock_price;
      } else {
        // currently the lob is empty, so use the latest stock price (>0) as
        // the current stock price
        baseline_stock_prices.push(baseline_latest_stock_price);
        while (baseline_stock_prices.size() > moving_window_size) {
          baseline_stock_prices.pop();
        }
      }
    }
    // Cancel old order to free cash
    if (previous_orders.size() == 30) {
      for (int i = 0; i < 10; i++) {
        Order ord = previous_orders.front();
        trader_api->SubmitCancel(ord.order_id_);
        VLOG(1) << "i= " << i << "\tCancel " << ord.order_id_;
        previous_orders.pop();
      }
    }
  }
}

int main(int argc, char **argv) {
  // GFLAGS and GLOG Parsing
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  FLAGS_logtostderr = 1;

  // Getting VM and CLoudEX config.
  std::ifstream config_fstream;
  config_fstream.open(FLAGS_configuration_path.c_str());
  if (!config_fstream.is_open()) {
    VLOG(1) << "Failed to Open: " << FLAGS_configuration_path << std::endl;
    return -1;
  }
  Json::Value root;
  config_fstream >> root;
  config_fstream.close();
  std::string gateway_ip = root["gateway_ip"].asString();
  std::string client_id = root["client_id"].asString();
  std::string client_token = root["client_token"].asString();
  std::string project_id = root["project_id"].asString();
  std::string bigtable_id = root["bigtable_id"].asString();
  std::string table_name = root["table_name"].asString();

  // start redis and flushall
  system("redis-server --daemonize yes");
  system("redis-cli flushall");

  // Start Trader API
  Trader *trader_api = new Trader(gateway_ip, client_id, client_token);

  std::vector<std::string> pair_symbols;
  pair_symbols.push_back("AA");
  pair_symbols.push_back("AB");

  trader_api->ConfigActiveSymbols(pair_symbols);

  std::thread *pair_trading_thread =
      new std::thread(PairsTradeFunc, trader_api, pair_symbols[0],
                      pair_symbols[1], FLAGS_moving_window, FLAGS_tick_length,
                      FLAGS_threshold, FLAGS_base_shares);

  pair_trading_thread->join();
  delete pair_trading_thread;
  delete trader_api;
}
