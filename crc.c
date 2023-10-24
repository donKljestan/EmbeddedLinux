#include <stdio.h>
const unsigned char POLY = 0xeb;
 /*u zavisnosti od policnoma mogu da se detektuju paterni gresaka
 CRC polynomials with specific Hamming Distance Properties- 0xeb detektuje HD=5
 0x83 detektuje HD=4, 0xe7 detektuje HD=3, a 0x9b detektuje HD=6
 */
unsigned char getCRC(unsigned char message[], unsigned char length)
{
  unsigned char i, j, crc = 0;
 
  for (i = 0; i < length; i++)
  {
    crc ^= message[i];
    for (j = 0; j < 8; j++)  /* Prepare to rotate 8 bits */
    {
      if (crc & 1)   /* if b15 is set... */
        crc = (crc >> 1) ^ POLY;  /* rotate and XOR with polynomic */
      crc >>= 1;  /* just rotate */
    }
  }
  return crc;
}
int main()
{
	unsigned char message[4] = {0x83, 0x01, 0x01, 0x01};
	unsigned char message1 = getCRC(message, 4);
	printf("%u", message1);
	return 0;
}