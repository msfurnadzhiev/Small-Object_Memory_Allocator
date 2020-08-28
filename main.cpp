#include <iostream>
#include <algorithm>
#include "MemoryAllocator.h"
using namespace std;
using namespace MemoryAllocator;

struct TestClass : public SmallObject {
  int x, y;

  void set_x(int x_, int y_) {
    x = x_;
    y = y_;
  }
};




int main() {

  SmallObject::get_instance();

  TestClass *t = new TestClass;
  t->set_x(42, 13);

  cout << t->x << " " << t->y << endl;

  SmallObject::operator delete(t,sizeof(TestClass));


  return 0;
}
