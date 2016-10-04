/**
* 
* @file
*
* @brief  TurboLZ packer support
*
* @author vitamin.caig@gmail.com
*
**/

//local includes
#include "container.h"
#include "pack_utils.h"
//common includes
#include <byteorder.h>
#include <contract.h>
#include <make_ptr.h>
#include <pointers.h>
//library includes
#include <binary/format_factories.h>
#include <formats/packed.h>
//std includes
#include <algorithm>
#include <iterator>
//boost includes
#include <boost/array.hpp>
//text includes
#include <formats/text/packed.h>

namespace Formats
{
namespace Packed
{
  namespace TurboLZ
  {
    const std::size_t MAX_DECODED_SIZE = 0xc000;

    struct Simple
    {
      static const String DESCRIPTION;
      static const std::size_t MIN_SIZE = 0x44;//TODO
      static const std::string DEPACKER_PATTERN;

#ifdef USE_PRAGMA_PACK
#pragma pack(push,1)
#endif
      PACK_PRE struct RawHeader
      {
        static const std::size_t FIXED_PART_SIZE = 0x16;

        //+0
        char Padding1;
        //+1
        uint16_t DepackerBodySrc;
        //+3
        char Padding2;
        //+4
        uint16_t DepackerBodyDst;
        //+6
        char Padding3;
        //+7
        uint16_t DepackerBodySize;
        //+0x9
        char Padding4[4];
        //+0xd
        uint16_t PackedDataSrc;
        //+0x0f
        char Padding5;
        //+0x10
        uint16_t PackedDataDst;
        //+0x12
        char Padding6;
        //+0x13
        uint16_t PackedDataSize;
        //+0x15
        char Padding7;
        //+0x16
        char DepackerBody[1];
        //+0x17
        uint8_t PackedDataCopyDirection;
        //+0x18
        char Padding8;
        //+0x19
        uint16_t DepackTarget;
        //+0x1b
        char Padding9;
        //+0x1c
        uint16_t DepackPreSource;
        //+0x1e
        char Padding10[0x25];
        //+0x43
        uint8_t LastByte;
        //+0x44
      } PACK_POST;

      struct KeyFunc : public std::unary_function<void, uint8_t>
      {
        KeyFunc(const uint8_t* /*data*/, std::size_t /*size*/)
        {
        }
        
        KeyFunc()
        {
        }

        uint8_t operator() ()
        {
          return 0;
        }
      };
#ifdef USE_PRAGMA_PACK
#pragma pack(pop)
#endif
    };

    struct Protected
    {
      static const String DESCRIPTION;
      static const std::size_t MIN_SIZE = 0x88;//TODO
      static const std::string DEPACKER_PATTERN;

#ifdef USE_PRAGMA_PACK
#pragma pack(push,1)
#endif
      PACK_PRE struct RawHeader
      {
        static const std::size_t FIXED_PART_SIZE = 0x19;

        //+0
        char Padding1;
        //+1
        uint16_t DepackerBodySrc;
        //+3
        char Padding2;
        //+4
        uint16_t DepackerBodyDst;
        //+6
        char Padding3;
        //+7
        uint16_t DepackerBodySize;
        //+0x9
        char Padding4[4];
        //+0xd
        uint16_t PackedDataSrc;
        //+0x0f
        char Padding5;
        //+0x10
        uint16_t PackedDataDst;
        //+0x12
        char Padding6;
        //+0x13
        uint16_t PackedDataSize;
        //+0x15
        char Padding7[4];
        //+0x19
        char DepackerBody[1];
        //+0x1a
        uint8_t PackedDataCopyDirection;
        //+0x1b
        char Padding8;
        //+0x1c
        uint16_t DepackTarget;
        //+0x1e
        char Padding9;
        //+0x1f
        uint16_t DepackPreSource;
        //+0x21
        char Padding10[0x2b];
        //+0x4c
        uint8_t LastByte;
        //+0x4d
        char Padding11[0x3a];
        //+0x87
        uint8_t InitialCryptoKeyIndex;
        //+0x88
      } PACK_POST;
      
      static const std::size_t KeyOffset = offsetof(RawHeader, DepackerBody);

      struct KeyFunc : public std::unary_function<void, uint8_t>
      {
        KeyFunc(const uint8_t* data, std::size_t size)
          : Index(Key[offsetof(RawHeader, InitialCryptoKeyIndex) - KeyOffset])
        {
          Require(KeyOffset + Key.size() <= size);
          std::copy(data + KeyOffset, data + KeyOffset + Key.size(), Key.begin());
        }

        uint8_t operator() ()
        {
          ++Index;
          Index &= 0x7f;
          return Key[Index];
        }
      private:
        boost::array<uint8_t, 128> Key;
        uint8_t& Index;
      };
#ifdef USE_PRAGMA_PACK
#pragma pack(pop)
#endif
    };

    const String Simple::DESCRIPTION = Text::TLZ_DECODER_DESCRIPTION;
    const std::string Simple::DEPACKER_PATTERN(
      "21??"          // ld hl,xxxx depacker body src
      "11??"          // ld de,xxxx depacker body dst
      "01??"          // ld bc,xxxx depacker body size
      "d5"            // push de
      "edb0"          // ldir
      "21??"          // ld hl,xxxx packed src
      "11??"          // ld de,xxxx packed dst
      "01??"          // ld bc,xxxx packed size
      "c9"            // ret
      //+0x16
      "ed?"           // ldir/lddr
      "11??"          // ld de,depack dst
      "21??"          // ld hl,depack src-1
      "23"            // inc hl
      "7e"            // ld a,(hl)
      "0f"            // rrca
      "30?"           // jr nc,xxx
      "77"            // ld (hl),a
      "e60f"          // and 0xf
    );

    const String Protected::DESCRIPTION = Text::TLZP_DECODER_DESCRIPTION;
    const std::string Protected::DEPACKER_PATTERN(
      "21??"          // ld hl,xxxx depacker body src
      "11??"          // ld de,xxxx depacker body dst
      "01??"          // ld bc,xxxx depacker body size
      "d5"            // push de
      "edb0"          // ldir
      "21??"          // ld hl,xxxx packed src
      "11??"          // ld de,xxxx packed dst
      "01??"          // ld bc,xxxx packed size
      "dde1"          // pop ix
      "dde9"          // jp ix
      //+0x19
      "ed?"           // ldir/lddr
      "11??"          // ld de,depack dst
      "21??"          // ld hl,depack src-1
      "23"            // inc hl
      "7e"            // ld a,(hl)
      "0f"            // rrca
      "d2??"          // jp nc,xxx
      "77"            // ld (hl),a
      "e60f"          // and 0xf
    );

    static_assert(sizeof(Simple::RawHeader) == 0x44, "Invalid layout");
    static_assert(sizeof(Protected::RawHeader) == 0x88, "Invalid layout");

    template<class Version>
    class Container
    {
    public:
      Container(const void* data, std::size_t size)
        : Data(static_cast<const uint8_t*>(data))
        , Size(size)
      {
      }

      bool FastCheck() const
      {
        if (Size < sizeof(typename Version::RawHeader))
        {
          return false;
        }
        const typename Version::RawHeader& header = GetHeader();
        const DataMovementChecker checker(fromLE(header.PackedDataSrc), fromLE(header.PackedDataDst), fromLE(header.PackedDataSize), header.PackedDataCopyDirection);
        if (!checker.IsValid())
        {
          return false;
        }
        if (checker.FirstOfMovedData() != std::size_t(fromLE(header.DepackPreSource) + 1))
        {
          return false;
        }
        if (GetPackedDataSize())
        {
          return true;
        }
        return false;
      }

      std::size_t GetPackedDataOffset() const
      {
        const typename Version::RawHeader& header = GetHeader();
        return header.FIXED_PART_SIZE + fromLE(header.DepackerBodySize);
      }

      std::size_t GetPackedDataSize() const
      {
        const typename Version::RawHeader& header = GetHeader();
        //some versions contains invalid PackedDataSize values
        const std::size_t depackerSize = GetPackedDataOffset();
        const std::size_t totalSize = depackerSize + fromLE(header.PackedDataSize);
        return std::min(totalSize, Size) - depackerSize;
      }

      const uint8_t* GetPackedData() const
      {
        const std::size_t offset = GetPackedDataOffset();
        return Data + offset;
      }

      const typename Version::RawHeader& GetHeader() const
      {
        assert(Size >= sizeof(typename Version::RawHeader));
        return *safe_ptr_cast<const typename Version::RawHeader*>(Data);
      }
      
      typename Version::KeyFunc GetKeyFunc() const
      {
        return typename Version::KeyFunc(Data, Size);
      }
    private:
      const uint8_t* const Data;
      const std::size_t Size;
    };

    template<class Version>
    class DataDecoder
    {
    public:
      explicit DataDecoder(const Container<Version>& container)
        : IsValid(container.FastCheck())
        , Header(container.GetHeader())
        , Stream(container.GetPackedData(), container.GetPackedDataSize())
        , Result(new Dump())
        , Decoded(*Result)
      {
        if (IsValid && !Stream.Eof())
        {
          typename Version::KeyFunc keyFunctor = container.GetKeyFunc();
          IsValid = DecodeData(keyFunctor);
        }
      }

      std::auto_ptr<Dump> GetResult()
      {
        return IsValid
          ? Result
          : std::auto_ptr<Dump>();
      }

      std::size_t GetUsedSize() const
      {
        return IsValid
          ? Header.FIXED_PART_SIZE + fromLE(Header.DepackerBodySize) + Stream.GetProcessedBytes()
          : 0;
      }
    private:
      template<class KeyFunc>
      bool DecodeData(KeyFunc& keyFunctor)
      {
        while (!Stream.Eof() && Decoded.size() < MAX_DECODED_SIZE)
        {
          const uint_t token = Stream.GetByte();
          if (!token)
          {
            //%00000000 - exit
            Decoded.push_back(Header.LastByte);
            Simple::KeyFunc noDecode;
            CopyNonPacked(Stream.GetRestBytes(), noDecode);
            return true;
          }
          else if (1 == (token & 1))
          {
            //%(nnn-3)YYYY1 yyyyyyyy
            //%111YYYY1 yyyyyyyy nnnnnnnn
            const uint_t offset = 256 * ((token & 30) >> 1) + Stream.GetByte();
            const uint_t len = (0xe0 == (token & 0xe0))
              ? Stream.GetByte()
              : 3 + (token >> 5);
            if (!CopyFromBack(offset + 1, Decoded, len))
            {
              return false;
            }
          }
          else if (2 == (token & 3))
          {
            //%(nnnnnn-3)10 bbbbbbbb
            //%11111110 bbbbbbbb nnnnnnnn
            //%11111110 bbbbbbbb 11111111 nnnnnnnn
            const uint_t initCount = token >> 2;
            const uint8_t data = Stream.GetByte();
            uint8_t incMarker = 63 + 3;
            for (uint_t len = initCount + 3; len;)
            {
              std::fill_n(std::back_inserter(Decoded), len, data);
              if (len != incMarker)
              {
                break;
              }
              len = Stream.GetByte();
              incMarker = 0xff;
            }
          }
          else
          {
            assert(0 == (token & 3));
            //%nnnnnn00 b8*n
            //%11111100 b8*63 0
            //%11111100 b8*63 [11111111 b8*255] nnnnnnnn b8*n
            const uint_t initCount = token >> 2;
            uint8_t incMarker = 63;
            for (uint_t len = initCount; len; )
            {
              if (!CopyNonPacked(len, keyFunctor))
              {
                //just exit, decode as much data as possible
                return true;
              }
              if (len != incMarker)
              {
                break;
              }
              len = Stream.GetByte();
              incMarker = 0xff;
            }
          }
        }
        return true;
      }
    private:
      template<class KeyFunc>
      bool CopyNonPacked(std::size_t len, KeyFunc& keyFunctor)
      {
        for (; len && !Stream.Eof(); --len)
        {
          const uint8_t data = Stream.GetByte();
          const uint8_t key = keyFunctor();
          Decoded.push_back(data ^ key);
        }
        return len == 0;
      }
    private:
      bool IsValid;
      const typename Version::RawHeader& Header;
      ByteStream Stream;
      std::auto_ptr<Dump> Result;
      Dump& Decoded;
    };
  }//namespace TurboLZ

  template<class Version>
  class TurboLZDecoder : public Decoder
  {
  public:
    TurboLZDecoder()
      : Depacker(Binary::CreateFormat(Version::DEPACKER_PATTERN, Version::MIN_SIZE))
    {
    }

    virtual String GetDescription() const
    {
      return Version::DESCRIPTION;
    }

    virtual Binary::Format::Ptr GetFormat() const
    {
      return Depacker;
    }

    virtual Container::Ptr Decode(const Binary::Container& rawData) const
    {
      if (!Depacker->Match(rawData))
      {
        return Container::Ptr();
      }
      const typename TurboLZ::Container<Version> container(rawData.Start(), rawData.Size());
      if (!container.FastCheck())
      {
        return Container::Ptr();
      }
      TurboLZ::DataDecoder<Version> decoder(container);
      return CreateContainer(decoder.GetResult(), decoder.GetUsedSize());
    }
  private:
    const Binary::Format::Ptr Depacker;
  };

  Decoder::Ptr CreateTurboLZDecoder()
  {
    return MakePtr<TurboLZDecoder<TurboLZ::Simple> >();
  }

  Decoder::Ptr CreateTurboLZProtectedDecoder()
  {
    return MakePtr<TurboLZDecoder<TurboLZ::Protected> >();
  }
}//namespace Packed
}//namespace Formats
