#ifndef COMMON_MESSAGE_TYPES_H_
#define COMMON_MESSAGE_TYPES_H_

#include <map>
#include <string>
#include <vector>

#include "common/message_primitives.h"
#include "common/parameters.h"
#include "common/utils.h"

// Order Class
class Order {
 public:
  std::string symbol_;     // Unique Stock Identifier (i.e. "AAPL", "GOOG")
  std::string order_id_;   // Unique Order Identifier (assigned at Gateway)
  std::string cancel_id_;  // ID to be cancelled ("NULL" if buy/sell/flush)
  std::string client_id_;  // Unique identifier for the client (i.e. "C1", "C2")
  OrderAction action_;     // One of {buy, sell, cancel, flush}
  OrderType type_;         // One of {limit, market, null}
  int num_shares_;         // Number of unfilled shares for this order.
  int limit_price_;        // Matching price threshold for this order.
  uint64_t genesis_timestamp_;  // Timestamp assigned by the Trader API
  uint64_t gateway_timestamp_;  // Timestamp assigned by the Gateway
  uint64_t enqueue_timestamp_;  // Timestamp assigned on Sequencer Enqueueing
  uint64_t dequeue_timestamp_;  // Timestamp assigned on Sequencer Dequeueing
  uint64_t order_serial_num_;   // Unique Sequence Number for Order

  OrderResult result_;  // The Result After the Order
  // Construct an Order from Serialized String
  explicit Order(const std::string& serialized_order);

  // Default Constructor
  Order();

  // Serialize Order as String
  std::string SerializeOrder(bool anon = false) const;

  // Timestamping Methods
  void AssignGenesisTimestamp();
  void AssignGatewayTimestamp();
  void AssignEnqueueTimestamp();
  void AssignDequeueTimestamp();

  // Generate Order ID and Populate Field in Object
  void GenerateOrderId(const std::string& gateway_id, uint64_t counter);

  void GenerateOrderId(const std::string& gateway_id,
                       const std::string& client_id, uint64_t counter);
  // Timestamp-Based Order Comparator
  bool operator<(const Order& order) const {
    return gateway_timestamp_ < order.gateway_timestamp_;
  }
  // To Use Small-Endian of Priority Queue, We Must Overload > Operator
  bool operator>(const Order& order) const {
    return gateway_timestamp_ > order.gateway_timestamp_;
  }
};

// Trade Class
class Trade {
 public:
  std::string symbol_;            // Unique Stock Identifier
  uint64_t buyer_serial_num_;     // Buyer Order Serial Number
  uint64_t seller_serial_num_;    // Seller Order Serial Number
  std::string buyer_order_id_;    // Buyer Order ID
  std::string seller_order_id_;   // Seller Order ID
  std::string buyer_client_id_;   // Client who submitted the Bid
  std::string seller_client_id_;  // Client who submitted the Ask
  int exec_price_;                // Price point at which the trade executed
  int cash_traded_;               // Cash exchanged by the buyer and seller
  int shares_traded_;             // Shares exchanged by the buyer and seller
  uint64_t creation_timestamp_;   // Timestamp assigned by the Trade Constructor
  uint64_t release_timestamp_;    // Timestamp designated for Release by H/R
  uint64_t trade_serial_num_;     // Unique Sequence Number for Trade

  // Construct a Trade from Two Matching Orders
  Trade(Order* incoming_trade, Order* queued_trade);

  // Construct a Trade from a Serialized String
  explicit Trade(const std::string& serialized_trade);

  // Default Constructor
  Trade();

  // Serialize Trade as String
  std::string SerializeTrade(bool anon_buyer = false,
                             bool anon_seller = false) const;

  // Timestamp-Based Trade Comparator
  bool operator<(const Trade& trade) const {
    return creation_timestamp_ < trade.creation_timestamp_;
  }
};

// Limit Order Book Class
class LimitOrderBook {
 public:
  std::string symbol_;                       // Unique Stock Identifier
  std::map<std::string, Order> buy_queue_;   // Buy Limit Order Book
  std::map<std::string, Order> sell_queue_;  // Sell Limit Order Book
  uint64_t creation_timestamp_;  // Timestamp assigned by the Serialize Method
  uint64_t release_timestamp_;   // Timestamp designated for Release by H/R

  // Construct a LimitOrderBook from Serialized String
  explicit LimitOrderBook(const std::string& serialized_book);

  // Default Constructor
  LimitOrderBook();

  // Serialize Trade as String
  std::string SerializeBook(int max_orders = 0, bool anon = false);
};

// Client Information Snapshot
class ClientInformationSnapshot {
 public:
  std::string client_id_;       // Unique Client Identifier
  uint64_t global_serial_num_;  // Global Trade Serial Number
  uint64_t order_serial_num_;   // Client-Specific Order Serial Number
  std::map<std::string, int> my_portfolio_;  // Client-Specific Portfolio
  std::vector<Order> outstanding_orders_;    // Client-Specific Orders in Book

  // Construct a ClientInformationSnapshot from Serialized String
  explicit ClientInformationSnapshot(const std::string& serialized_snapshot);

  // Default Constructor
  ClientInformationSnapshot();

  // Serialize Trade as String
  std::string SerializeSnapshot() const;
};

#endif  // COMMON_MESSAGE_TYPES_H_
