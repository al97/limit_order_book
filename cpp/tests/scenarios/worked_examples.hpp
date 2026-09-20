#pragma once

#include "command.hpp"

namespace lob::test {

inline Commands GoldenNonCrossingRest() {
  return {SubmitGtc(1, lob::Side::Buy, 100, 10)};
}

inline Commands GoldenCrossAtMakerPrice() {
  return {
      SubmitGtc(1, lob::Side::Sell, 100, 3),
      SubmitGtc(2, lob::Side::Buy, 101, 3),
  };
}

inline Commands GoldenFifoAtOnePrice() {
  return {
      SubmitGtc(1, lob::Side::Sell, 100, 3),
      SubmitGtc(2, lob::Side::Sell, 100, 5),
      SubmitGtc(3, lob::Side::Buy, 100, 4),
  };
}

inline Commands PlainEnglishWorkedExample() {
  return {
      SubmitGtc(10, lob::Side::Sell, 100, 3),
      SubmitGtc(11, lob::Side::Sell, 100, 5),
      SubmitGtc(12, lob::Side::Sell, 101, 10),
      SubmitGtc(20, lob::Side::Buy, 101, 6),
  };
}

inline Commands TechnicalNumericExample() {
  return {
      SubmitGtc(1, lob::Side::Buy, 10050, 40),
      SubmitGtc(2, lob::Side::Buy, 10000, 80),
      SubmitGtc(3, lob::Side::Sell, 10100, 50),
      SubmitGtc(4, lob::Side::Sell, 10100, 30),
      SubmitGtc(5, lob::Side::Sell, 10150, 100),
      SubmitGtc(6, lob::Side::Buy, 10100, 70),
      SubmitGtc(7, lob::Side::Buy, 10150, 200),
      CancelOrder(1),
  };
}

inline Commands CancelHeadMiddleTail() {
  return {
      SubmitGtc(1, lob::Side::Buy, 100, 3),
      SubmitGtc(2, lob::Side::Buy, 100, 4),
      SubmitGtc(3, lob::Side::Buy, 100, 5),
      CancelOrder(2),
      SubmitGtc(4, lob::Side::Sell, 100, 1),
      CancelOrder(1),
      CancelOrder(3),
  };
}

}  // namespace lob::test
