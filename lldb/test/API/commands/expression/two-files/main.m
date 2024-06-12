#import <Foundation/Foundation.h>

extern int AddElement (char *value);
extern char *GetElement (int idx);
extern void *GetArray();

int
main ()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  int idx = AddElement ("some string");
  void *array_token = GetArray();

  char *string = GetElement (0); // Set breakpoint here, then do 'expr (NSArray*)array_token'.
  if (string)
    __builtin_printf ("This: %s.\n", string);

  [pool release];
  return 0;
}  
