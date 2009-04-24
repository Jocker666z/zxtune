#ifndef __TOOLS_H_DEFINED__
#define __TOOLS_H_DEFINED__

#include <iterator>
#include <algorithm>

template<class T, std::size_t D>
inline std::size_t ArraySize(const T (&)[D])
{
  return D;
}

template<class T, std::size_t D>
inline const T* ArrayEnd(const T (&c)[D])
{
  return c + D;
}

template<class T, std::size_t D>
inline T* ArrayEnd(T (&c)[D])
{
  return c + D;
}

template<class T, class F>
inline T safe_ptr_cast(F* from)
{
  return static_cast<T>(static_cast<void*>(from));
}

template<class T, class F>
inline T safe_ptr_cast(const F* from)
{
  return static_cast<T>(static_cast<const void*>(from));
} 

template<class C>
class cycled_iterator
{
public:
  cycled_iterator() : begin(), end(), cur()
  {
  }

  cycled_iterator(C start, C stop) : begin(start), end(stop), cur(start)
  {
  }

  cycled_iterator(const cycled_iterator<C>& rh) : begin(rh.begin), end(rh.end), cur(rh.cur)
  {
  }

  cycled_iterator<C>& operator = (const cycled_iterator<C>& rh)
  {
    begin = rh.begin;
    end = rh.end;
    cur = rh.cur;
    return *this;
  }

  bool operator == (const cycled_iterator<C>& rh) const
  {
    return begin == rh.begin && end == rh.end && cur == rh.cur;
  }

  cycled_iterator<C>& operator ++ ()
  {
    if (end == ++cur)
    {
      cur = begin;
    }
    return *this;
  }

  cycled_iterator<C>& operator -- ()
  {
    if (begin == cur)
    {
      cur = end;
    }
    --cur;
    return *this;
  }

  typename std::iterator_traits<C>::reference operator * () const
  {
    return *cur;
  }

  typename std::iterator_traits<C>::pointer operator ->() const
  {
    return &*cur;
  }
private:
  C begin;
  C end;
  C cur;
};

template<class T>
inline T clamp(T val, T min, T max)
{
  return std::min<T>(std::max<T>(val, min), max);
}

#endif //__TOOLS_H_DEFINED__
