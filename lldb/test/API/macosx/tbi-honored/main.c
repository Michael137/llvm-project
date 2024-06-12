#include <stdint.h>
union ptrbytes {
  int *p;
  uint8_t bytes[8];
};
int main() {
  int c = 15;
  union ptrbytes pb;
  pb.p = &c;
  pb.bytes[7] = 0xfe;
  __builtin_printf("%d\n", *pb.p); // break here
}
