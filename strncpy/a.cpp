/* strncpy example */
#include <stdio.h>
#include <string.h>
#include <iostream>

int main ()
{
  char z='V';
  char str1[]= "0123456789";
  char x='V';
  char str2[5];
  char y='V';

  /* copy to sized buffer (overflow safe): */
  strncpy ( str2, str1, sizeof(str2) );

  std::cout << x << " " << y << " " << z << std::endl;
  std::cout << str1 << std::endl;
  std::cout << str2 << std::endl;


  return 0;
}
