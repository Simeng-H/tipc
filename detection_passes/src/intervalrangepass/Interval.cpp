#include <set>
#include <cmath>
#include <iostream>
#include "Interval.h"

#include "llvm/Support/raw_ostream.h"

using namespace llvm;

using namespace interval;

// Simple builder and accessor for pair representation
Interval interval::make(double l, double r) {
  // errs() << "make: " << l << " " << r << "\n";
  auto p = std::make_pair(l,r);
  // errs() << "make: " << str(p) << "\n";
  return p;
  // return std::make_pair(l,r); 
}
double interval::lower(Interval i) { return i.first; }
double interval::upper(Interval i) { return i.second; }

// Pre-defined intervals 
Interval interval::full() { return make(minf,pinf); }
Interval interval::empty() {
  auto p = make(pinf,minf);
  // errs() << "empty: " << str(p) << "\n";
  return p; 
   
}
Interval interval::unit() { return make(0,1); }

// helper function to check double equality
bool double_eq(double l, double r) {
  return std::abs(l-r) < std::numeric_limits<double>::epsilon();
  // return fabs(l - r) <= ( (fabs(l) > fabs(r) ? fabs(r) : fabs(l)) * std::numeric_limits<double>::epsilon());
}

/* Least Upper Bound
 *   Edge cases lead to extreme intervals or extreme bounds.
 *   General case takes the lowest of the lows and the highest of the highs
 *   as the bounds.
 */
Interval interval::lub(Interval l, Interval r) {
  Interval result;
  if (l == full()) {
    result = full(); 
  } else if (l == empty()) {
    result = r;
  } else if (lower(l) == minf && upper(r) == pinf) {
    result = full();
  } else if (lower(l) == minf) {
    result = make(minf, std::max(upper(l), upper(r)));
  } else if (upper(l) == pinf) {
    result = make(std::min(lower(l), lower(r)), pinf);
  } else {
    result = make(std::min(lower(l), lower(r)), std::max(upper(l), upper(r)));
  }
  return result;
}

/* Unary negation
 *  Numerous special cases where the extreme bounds are involved. 
 *  General case is to negate the bounds and use min/max to establish bounds.
 */
Interval interval::neg(Interval i) {
  Interval result;
  if (minf == lower(i) && pinf == upper(i)) {
    result = full();
  } else if (pinf == lower(i) && minf == upper(i)) {
    result = empty();
  } else if (minf == lower(i) && minf == upper(i)) {
    result = make(pinf, pinf);
  } else if (pinf == lower(i) && pinf == upper(i)) {
    result = make(minf, minf);
  } else if (pinf == upper(i)) {
    result = make(minf, -(lower(i)));
  } else if (minf == lower(i)) {
    result = make(-(upper(i)), pinf);
  } else {
    result = make(std::min(-(upper(i)),-(lower(i))), 
                  std::max(-(upper(i)),-(lower(i))));
  }
  return result;
}

/* Addition
 *  Edge cases for empty intervals and maximal bounds
 *  General case is to add the corresponding bounds.
 */
Interval interval::add(Interval l, Interval r) {
  double low, up;

  if (pinf == lower(l) || pinf == lower(r)) {
    low = pinf; // one of the arguments is empty
  } else if (minf == lower(l) || minf == lower(r)) {
    low = minf; 
  } else { 
    low = lower(l) + lower(r); 
  } 

  if (minf == upper(l) || minf == upper(r)) {
    up = minf; // one of the arguments is empty
  } else if (pinf == upper(l) || pinf == upper(r)) {
    up = pinf; 
  } else { 
    up = upper(l) + upper(r); 
  } 

  return make(low, up);
}

Interval interval::sub(Interval l, Interval r) {
  return interval::add(l, interval::neg(r));
}

/* Multiplication
 */
Interval interval::mul(Interval l, Interval r) {
  if (lower(l)>upper(l) || lower(r)>upper(r)) {
    return empty(); // one of the arguments is empty
  }

  // calculate the 4 possible bounds
  double ll = lower(l) * lower(r);
  double lu = lower(l) * upper(r);
  double ul = upper(l) * lower(r);
  double uu = upper(l) * upper(r);

  // put the bounds in a set to remove duplicates
  std::set<double> bounds = {ll, lu, ul, uu};

  // handle the cases where nan is involved (e.g. 0 * inf), such values are changed to 0
  for (auto it = bounds.begin(); it != bounds.end(); ++it) {
    auto this_bound = *it;
    if (isnan(this_bound)) {
      bounds.erase(it);
      bounds.insert(0);
    }
  }

  // min and max of the bounds
  double low = *bounds.begin();
  double up = *bounds.rbegin();

  return interval::make(low, up);
}

/* Division
 */
Interval interval::div(Interval l, Interval r) {
  if (pinf == lower(l) || pinf == lower(r) || minf == upper(l) || minf == upper(r)) {
    return empty(); // one of the arguments is empty
  }
  Interval result;

  // TODO: fix everything except the last case by removing divisions
  // find l * 1/r, cases based on whether r contains 0
  if (lower(r) <= 0 && upper(r) >= 0) {
    // r = [l, u] and contains 0
    Interval left_r_reciprocal = interval::make(minf, 1/lower(r));
    Interval right_r_reciprocal = interval::make(1/upper(r), pinf);
    result = interval::lub(interval::mul(l, left_r_reciprocal), 
                         interval::mul(l, right_r_reciprocal));
  } else if (upper(r) == 0) {
    // r = [0, u]
    Interval r_reciprocal = interval::make(minf, 1/lower(r));
    result = interval::mul(l, r_reciprocal);
  } else if (lower(r) == 0) {
    // r = [l, 0]
    Interval r_reciprocal = interval::make(1/upper(r), pinf);
    result = interval::mul(l, r_reciprocal);
  } else {
    // r = [l, u] and does not contain 0
    Interval r_reciprocal = interval::make(1/upper(r), 1/lower(r));
    result = interval::mul(l, r_reciprocal);
  }

  return make(floor(lower(result)), ceil(upper(result)));
  
}

/* Comparison Operators
 *   Trivial imprecise definitions
 */
Interval interval::lt(Interval l, Interval r) {
  // return empty();
  if (pinf == lower(l) || pinf == lower(r) || minf == upper(l) || minf == upper(r)) {
    return empty(); // one of the arguments is empty
  }

  // if left is definitely less than right, return true
  if (upper(l) < lower(r)) {
    return make(1, 1);
  }

  // if left is definitely greater than right, return false
  if (upper(r) < lower(l)) {
    return make(0, 0);
  }

  // otherwise, we don't know
  return unit();

}
Interval interval::gt(Interval l, Interval r) {
  return interval::lt(r, l);
}
Interval interval::eq(Interval l, Interval r) {
  if (pinf == lower(l) || pinf == lower(r) || minf == upper(l) || minf == upper(r)) {
    return empty(); // one of the arguments is empty
  }

  // if one is definitely less than the other, they are not equal
  if (upper(l) < lower(r) || upper(r) < lower(l)) {
    return make(0, 0);
  }

  // if both are singletons, we can check if they are equal
  if (double_eq(lower(l), upper(l)) && double_eq(lower(r), upper(r))) {
    if (double_eq(lower(l), lower(r))) {
      return make(1, 1);
    } else {
      return make(0, 0);
    }
  }
  
  // otherwise, we don't know
  return unit();
}
Interval interval::ne(Interval l, Interval r) {
  if (pinf == lower(l) || pinf == lower(r) || minf == upper(l) || minf == upper(r)) {
    return empty(); // one of the arguments is empty
  }

  // if they are definitely equal, they are not not equal
  if (eq(l, r) == make(1, 1)) {
    return make(0, 0);
  }

  // if they are definitely not equal, they are not equal
  if (eq(l, r) == make(0, 0)) {
    return make(1, 1);
  }

  // otherwise, we don't know
  return unit();
}

std::string istr(double b, bool round_up = false) {
  std::string result = "";
  // errs() << "b: " << b << "\n";
  // errs() << "b == inf: " << (b == pinf) << "\n";
  // errs() << "b == -inf: " << (b == minf) << "\n";
  // errs() << "b == inf: " << double_eq(b, pinf) << "\n";
  // errs() << "b == -inf: " << double_eq(b, minf) << "\n";
  if (b == minf) {
    result = "-inf";
  } else if (b == pinf) {
    result = "+inf"; 
  } else if (round_up) {
    int rounded = (int) ceil(b);
    result = std::to_string(rounded);
  } else {
    int rounded = (int) floor(b);
    result = std::to_string(rounded);
  }
  return result;
}

std::string interval::str(Interval i) {
  std::string f = istr(lower(i), false);
  std::string s = istr(upper(i), true);
  return "[" + f + "," + s + "]";
}

// Deep equality for intervals
bool interval::operator==(Interval l, Interval r) {
  // handle cases where infinity is involved
  // errs() << "comparing " << interval::str(l) << " and " << interval::str(r) << "\n";
  bool le = 0, ue = 0, linf = 0, uinf = 0, result = 0;
  if (lower(l) == minf || lower(r) == minf || lower(l) == pinf || lower(r) == pinf) {
    le = (lower(l) == lower(r));
    // errs() << "\tcomparing " << lower(l) << " and " << lower(r) << " result: " << le << "\n";
    linf = 1;
  }
  if (upper(l) == minf || upper(r) == minf || upper(l) == pinf || upper(r) == pinf) {
    ue = (upper(l) == upper(r));
    // errs() << "\tcomparing " << upper(l) << " and " << upper(r) << " result: " << ue << "\n";
    uinf = 1;
  }

  if (linf && uinf) {
    result = le && ue;
  } else if (linf) {
    result = le && double_eq(upper(l), upper(r));
  } else if (uinf) {
    result = ue && double_eq(lower(l), lower(r));
  } else {
    result = double_eq(lower(l), lower(r)) && double_eq(upper(l), upper(r));
  }

  errs() << "result: " << result << "\n";
  return result;
}

bool interval::operator!=(Interval l, Interval r) {
  return !(l == r);
}

