#include <algorithm>
#include <mutex>

#include "trader/trader_api.h"

#define HIGHEST_SELL_PRICE 99999999
#define LOWEST_BUY_PRICE 0

// Google Command Flags
/* Setup and identity flags */

DEFINE_string(configuration_path, "/root/vm_config.json",
              "Read your configuration file from this path");
DEFINE_int32(base_shares, 5000, "The base shares for mean reversion traders");
DEFINE_int32(moving_window, 5,
             "The window length (seconds) (for mean reversion)");
DEFINE_int32(tick_length, 1,
             "The basic time unit for moving window (seconds), that means, "
             "after how much time should we record one point of stock price");
DEFINE_double(threshold, 5, "The threshold (for mean reversion)");

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
  // Only Consider the lowest price
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

void MeanReversionFunc(Trader *trader_api, std::string target_symbol,
                       uint32_t moving_window_size, uint32_t tick_length,
                       double threshold, int base_shares) {
  std::vector<LimitOrderBook> recent_lobs;
  uint64_t start_timestamp = utils::GetMicrosecondTimestamp();
  uint64_t last_seen_timestamp = 1;
  std::queue<double> stock_prices;
  double latest_stock_price = 1;
  std::queue<Order> previous_orders;
  while (run) {
    VLOG(1) << "start_timestamp =" << start_timestamp
            << " current_time = " << utils::GetMicrosecondTimestamp()
            << " tick_length=" << tick_length;
    while (utils::GetMicrosecondTimestamp() <
           start_timestamp + tick_length * 1000 * 1000) {
      usleep(100);
    }
    start_timestamp = utils::GetMicrosecondTimestamp();
    VLOG(1) << "New Loop StartTimestamp-1 =" << start_timestamp;
    // The top one is the most recent one
    trader_api->GetRecentLOBs(target_symbol, &recent_lobs, last_seen_timestamp);
    if (recent_lobs.size() > 0) {
      last_seen_timestamp = recent_lobs[0].creation_timestamp_;
      // Take the highest buy price in the most recent lob as the stock price
      double highest_buy_price = GetHighestBuyPrice(&recent_lobs);
      double lowest_sell_price = GetLowestSellPrice(&recent_lobs);
      double current_stock_price = highest_buy_price;
      if (current_stock_price > LOWEST_BUY_PRICE) {
        if (stock_prices.size() >= moving_window_size) {
          // We have enough history now, calculate the mean price value based on
          // the past records
          std::queue<double> stock_prices_replica = stock_prices;
          double sum_price = 0;
          while (!stock_prices_replica.empty()) {
            sum_price += stock_prices_replica.front();
            stock_prices_replica.pop();
          }
          double average_price = sum_price / stock_prices.size();
          VLOG(1) << target_symbol << "\taverage_price=" << average_price
                  << "\tcurrent_stock_price=" << current_stock_price;
          // Place Order
          Order ord;
          if (current_stock_price > (1 + threshold / 100) * average_price &&
              lowest_sell_price < HIGHEST_SELL_PRICE) {
            // Sell
            int num_shares = static_cast<int>(current_stock_price /
                                              average_price * base_shares);
            LOG(ERROR) << target_symbol << "\t" << start_timestamp
                       << ": Sell Triggered: " << target_symbol << "\t"
                       << num_shares << "\t Current Price"
                       << current_stock_price << "\t AvgPrice" << average_price;
            // If I really want to sell, I should sell lower than anyone else
            trader_api->SubmitOrder(target_symbol, &ord, OrderType::limit,
                                    OrderAction::sell, num_shares,
                                    lowest_sell_price - 1);
            if (ord.order_id_ != "NULL") {
              previous_orders.push(ord);
            }
            LOG(ERROR) << "Submitted Selling Order " << ord.SerializeOrder();
          } else if (current_stock_price <
                     (1 - threshold / 100) * average_price) {
            // Buy
            int num_shares = static_cast<int>(
                average_price / current_stock_price * base_shares);
            LOG(ERROR) << target_symbol << "\t" << start_timestamp
                       << ": Buy Triggered: " << target_symbol << "\t"
                       << num_shares << "\t Current Price"
                       << current_stock_price << "\t AvgPrice" << average_price;
            // If I really want to buy, I should buy higher than anyone else
            trader_api->SubmitOrder(target_symbol, &ord, OrderType::limit,
                                    OrderAction::buy, num_shares,
                                    highest_buy_price + 1);
            if (ord.order_id_ != "NULL") {
              previous_orders.push(ord);
            }
            LOG(ERROR) << "Submitted buying Order " << ord.SerializeOrder();
          }
        }
        VLOG(1) << target_symbol << "\tPushed Price " << current_stock_price;
        stock_prices.push(current_stock_price);
        while (stock_prices.size() > moving_window_size) {
          stock_prices.pop();
        }
        latest_stock_price = current_stock_price;
      } else {
        // currently the lob is empty, so use the latest stock price (>0) as the
        // current stock price
        VLOG(1) << target_symbol << ": Order Empty in this LOB";
        VLOG(1) << target_symbol << ": Pushed Price (latest) "
                << latest_stock_price;
        stock_prices.push(latest_stock_price);
        while (stock_prices.size() > moving_window_size) {
          stock_prices.pop();
        }
      }

    } else {
      VLOG(1) << target_symbol << ": LOB Empty";
      stock_prices.push(latest_stock_price);
      while (stock_prices.size() > moving_window_size) {
        stock_prices.pop();
        VLOG(1) << target_symbol
                << "moving_window_size = " << moving_window_size;
      }
      VLOG(1) << target_symbol << ": LOB Empty - Popped Finished";
    }
    if (previous_orders.size() == 30) {
      for (int i = 0; i < 10; i++) {
        Order ord = previous_orders.front();
        trader_api->SubmitCancel(ord.order_id_);
        VLOG(1) << "i= " << i << "\tCancel " << ord.order_id_;
        previous_orders.pop();
        VLOG(1) << "i= " << i << "\tAfter Pop " << ord.order_id_;
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

  std::vector<std::string> target_symbols;
  target_symbols.push_back("AA");

  std::vector<std::thread *> mean_reversion_threads(target_symbols.size());
  trader_api->ConfigActiveSymbols(target_symbols);

  for (int i = 0; i < target_symbols.size(); i++) {
    mean_reversion_threads[i] = new std::thread(
        MeanReversionFunc, trader_api, target_symbols[i], FLAGS_moving_window,
        FLAGS_tick_length, FLAGS_threshold, FLAGS_base_shares);
  }
  for (int i = 0; i < target_symbols.size(); i++) {
    mean_reversion_threads[i]->join();
  }

  delete trader_api;
}
