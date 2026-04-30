#include "dpc/context.h"

int main(int argc, const char** argv) {
  dpc::Context ctx(0, 10);

  ctx.print();
  return 0;
}