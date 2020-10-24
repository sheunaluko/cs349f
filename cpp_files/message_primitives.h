#ifndef COMMON_MESSAGE_PRIMITIVES_H_
#define COMMON_MESSAGE_PRIMITIVES_H_

#include <string>

// Enumerated Types
enum class OrderType { limit, market, null };
enum class OrderAction { buy, sell, cancel, flush };
enum class OrderResult {
  valid,
  invalid,
  malformed,
  duplicate,
  error,
  flushed,
  authorization_error,
  network_error,
  window_exceeed,
  in_gateway,
  in_sequencer,
  me_shut,
  unknown
};
enum class Suffix { order, hold_trade, trade, hold_book, book };

// OrderAction Serialization
inline char SerializeAction(OrderAction action) {
  switch (action) {
    case OrderAction::buy:
      return 'B';
    case OrderAction::sell:
      return 'S';
    case OrderAction::cancel:
      return 'C';
    default:
      return 'F';
  }
}

// OrderAction Deserialization
inline OrderAction DeserializeAction(char action) {
  switch (action) {
    case 'B':
      return OrderAction::buy;
    case 'S':
      return OrderAction::sell;
    case 'C':
      return OrderAction::cancel;
    default:
      return OrderAction::flush;
  }
}

// OrderType Serialization
inline char SerializeType(OrderType type) {
  switch (type) {
    case OrderType::limit:
      return 'L';
    case OrderType::market:
      return 'M';
    default:
      return 'N';
  }
}

// OrderType Deserialization
inline OrderType DeserializeType(char type) {
  switch (type) {
    case 'L':
      return OrderType::limit;
    case 'M':
      return OrderType::market;
    default:
      return OrderType::null;
  }
}

// OrderResult Serialization
inline char SerializeResult(OrderResult result) {
  switch (result) {
    case OrderResult::valid:
      return 'V';
    case OrderResult::invalid:
      return 'I';
    case OrderResult::malformed:
      return 'M';
    case OrderResult::duplicate:
      return 'D';
    case OrderResult::flushed:
      return 'F';
    case OrderResult::authorization_error:
      return 'A';
    case OrderResult::network_error:
      return 'N';
    case OrderResult::window_exceeed:
      return 'W';
    case OrderResult::in_gateway:
      return 'G';
    case OrderResult::me_shut:
      return 'S';
    case OrderResult::in_sequencer:
      return 'Q';
    case OrderResult::unknown:
      return 'U';
    default:
      return 'E';
  }
}

// OrderResult Deserialization
inline OrderResult DeserializeResult(char result) {
  switch (result) {
    case 'V':
      return OrderResult::valid;
    case 'I':
      return OrderResult::invalid;
    case 'M':
      return OrderResult::malformed;
    case 'D':
      return OrderResult::duplicate;
    case 'F':
      return OrderResult::flushed;
    case 'A':
      return OrderResult::authorization_error;
    case 'N':
      return OrderResult::network_error;
    case 'W':
      return OrderResult::window_exceeed;
    case 'G':
      return OrderResult::in_gateway;
    case 'S':
      return OrderResult::me_shut;
    case 'Q':
      return OrderResult::in_sequencer;
    case 'U':
      return OrderResult::unknown;
    default:
      return OrderResult::error;
  }
}

// ZMQ Topic Suffix Serialization
inline std::string SerializeSuffix(Suffix suffix) {
  switch (suffix) {
    case Suffix::hold_book:
      return "_HOLD_BOOK";
    case Suffix::hold_trade:
      return "_HOLD_TRADE";
    case Suffix::book:
      return "_RELEASE_BOOK";
    case Suffix::trade:
      return "_RELEASE_TRADE";
    default:
      return "_ORDER";
  }
}

// ZMQ Topic Suffix Deserialization
inline Suffix DeserializeSuffix(std::string suffix) {
  if (suffix == "_HOLD_BOOK") return Suffix::hold_book;
  if (suffix == "_HOLD_TRADE") return Suffix::hold_trade;
  if (suffix == "_RELEASE_BOOK") return Suffix::book;
  if (suffix == "_RELEASE_TRADE") return Suffix::trade;
  return Suffix::order;
}

// Return Opposite Order Action
inline OrderAction flip(OrderAction action) {
  if (action == OrderAction::buy) return OrderAction::sell;
  if (action == OrderAction::sell) return OrderAction::buy;
  return action;
}

#endif  // COMMON_MESSAGE_PRIMITIVES_H_
