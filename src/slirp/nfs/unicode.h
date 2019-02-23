#ifndef _UNICODE_H_
#define _UNICODE_H_

bool WideToChar(const wchar_t *Src,char *Dest, size_t DestSize);
bool CharToWide(const char *Src,wchar_t *Dest,size_t DestSize);

#endif //_UNICODE_H_

