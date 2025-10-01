/*
 *  sha1.c
 *
 *  Description:
 *      This file implements the Secure Hashing Algorithm 1 as
 *      defined in FIPS PUB 180-1 published April 17, 1995.
 *
 *      The SHA-1, produces a 160-bit message digest for a given
 *      data stream.  It should take about 2**n steps to find a
 *      message with the same digest as a given message and
 *      2**(n/2) to find any two messages with the same digest,
 *      when n is the digest size in bits.  Therefore, this
 *      algorithm can serve as a means of providing a
 *      "fingerprint" for a message.
 *
 *  Portability Issues:
 *      SHA-1 is defined in terms of 32-bit "words".  This code
 *      uses <cstdint> to define 32 and 8 bit unsigned integer types.
 *
 *  Caveats:
 *      SHA-1 is designed to work with messages less than 2^64 bits
 *      long.  Although SHA-1 allows a message digest to be generated
 *      for messages of any number of bits less than 2^64, this
 *      implementation only works with messages with a length that is
 *      a multiple of the size of an 8-bit character.
 *
 */

#include "sha1.hh"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

namespace {
    /*
     *  Define the SHA1 circular left shift as a constexpr function
     */
    constexpr uint32_t SHA1CircularShift(int bits, uint32_t word) noexcept {
        return ((word << bits) | (word >> (32 - bits)));
    }
}

/*
 *  SHA1Context Constructor
 *
 *  Description:
 *      Initializes the SHA1Context in preparation for computing
 *      a new SHA1 message digest.
 */
SHA1Context::SHA1Context() noexcept
    : Intermediate_Hash{{0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0}}
    , Length_Low{0}
    , Length_High{0}
    , Message_Block_Index{0}
    , Message_Block{}
    , Computed{false}
    , Corrupted{0}
{
}

/*
 *  SHA1Context::Reset
 *
 *  Description:
 *      Resets the SHA1Context to initial state for computing
 *      a new SHA1 message digest.
 *
 *  Returns:
 *      sha Error Code.
 */
int SHA1Context::Reset() noexcept
{
    Length_Low = 0;
    Length_High = 0;
    Message_Block_Index = 0;

    Intermediate_Hash[0] = 0x67452301;
    Intermediate_Hash[1] = 0xEFCDAB89;
    Intermediate_Hash[2] = 0x98BADCFE;
    Intermediate_Hash[3] = 0x10325476;
    Intermediate_Hash[4] = 0xC3D2E1F0;

    Computed = false;
    Corrupted = 0;

    return shaSuccess;
}

/*
 *  SHA1Context::Result
 *
 *  Description:
 *      Returns the 160-bit message digest into the Message_Digest array
 *      provided by the caller.
 *      NOTE: The first octet of hash is stored in the 0th element,
 *            the last octet of hash in the 19th element.
 *
 *  Parameters:
 *      Message_Digest: [out]
 *          Where the digest is returned.
 *
 *  Returns:
 *      sha Error Code.
 */
int SHA1Context::Result(uint8_t* Message_Digest) noexcept
{
    if (!Message_Digest)
    {
        return shaNull;
    }

    if (Corrupted)
    {
        return Corrupted;
    }

    if (!Computed)
    {
        PadMessage();
        
        // Message may be sensitive, clear it out
        Message_Block.fill(0);
        
        Length_Low = 0;
        Length_High = 0;
        Computed = true;
    }

    for (size_t i = 0; i < SHA1HashSize; ++i)
    {
        Message_Digest[i] = static_cast<uint8_t>(
            Intermediate_Hash[i >> 2] >> (8 * (3 - (i & 0x03)))
        );
    }

    return shaSuccess;
}

/*
 *  SHA1Context::Input
 *
 *  Description:
 *      Accepts an array of octets as the next portion of the message.
 *
 *  Parameters:
 *      message_array: [in]
 *          An array of characters representing the next portion of
 *          the message.
 *      length: [in]
 *          The length of the message in message_array
 *
 *  Returns:
 *      sha Error Code.
 */
int SHA1Context::Input(const uint8_t* message_array, unsigned int length) noexcept
{
    if (length == 0)
    {
        return shaSuccess;
    }

    if (!message_array)
    {
        return shaNull;
    }

    if (Computed)
    {
        Corrupted = shaStateError;
        return shaStateError;
    }

    if (Corrupted)
    {
        return Corrupted;
    }

    while (length-- && !Corrupted)
    {
        Message_Block[Message_Block_Index++] = (*message_array & 0xFF);

        Length_Low += 8;
        if (Length_Low == 0)
        {
            Length_High++;
            if (Length_High == 0)
            {
                /* Message is too long */
                Corrupted = shaInputTooLong;
            }
        }

        if (Message_Block_Index == SHA1MessageBlockSize)
        {
            ProcessMessageBlock();
        }

        message_array++;
    }

    return shaSuccess;
}

/*
 *  SHA1Context::ProcessMessageBlock
 *
 *  Description:
 *      Processes the next 512 bits of the message stored in the
 *      Message_Block array.
 *
 *  Comments:
 *      Many of the variable names in this code, especially the
 *      single character names, were used because those were the
 *      names used in the publication.
 */
void SHA1Context::ProcessMessageBlock() noexcept
{
    constexpr std::array<uint32_t, 4> K = {
        0x5A827999,
        0x6ED9EBA1,
        0x8F1BBCDC,
        0xCA62C1D6
    };

    std::array<uint32_t, 80> W{};
    uint32_t A, B, C, D, E;

    // Initialize the first 16 words in the array W
    for (int t = 0; t < 16; t++)
    {
        W[t] = static_cast<uint32_t>(Message_Block[t * 4]) << 24;
        W[t] |= static_cast<uint32_t>(Message_Block[t * 4 + 1]) << 16;
        W[t] |= static_cast<uint32_t>(Message_Block[t * 4 + 2]) << 8;
        W[t] |= static_cast<uint32_t>(Message_Block[t * 4 + 3]);
    }

    for (int t = 16; t < 80; t++)
    {
        W[t] = SHA1CircularShift(1, W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16]);
    }

    A = Intermediate_Hash[0];
    B = Intermediate_Hash[1];
    C = Intermediate_Hash[2];
    D = Intermediate_Hash[3];
    E = Intermediate_Hash[4];

    for (int t = 0; t < 20; t++)
    {
        uint32_t temp = SHA1CircularShift(5, A) +
                        ((B & C) | ((~B) & D)) + E + W[t] + K[0];
        E = D;
        D = C;
        C = SHA1CircularShift(30, B);
        B = A;
        A = temp;
    }

    for (int t = 20; t < 40; t++)
    {
        uint32_t temp = SHA1CircularShift(5, A) + (B ^ C ^ D) + E + W[t] + K[1];
        E = D;
        D = C;
        C = SHA1CircularShift(30, B);
        B = A;
        A = temp;
    }

    for (int t = 40; t < 60; t++)
    {
        uint32_t temp = SHA1CircularShift(5, A) +
                        ((B & C) | (B & D) | (C & D)) + E + W[t] + K[2];
        E = D;
        D = C;
        C = SHA1CircularShift(30, B);
        B = A;
        A = temp;
    }

    for (int t = 60; t < 80; t++)
    {
        uint32_t temp = SHA1CircularShift(5, A) + (B ^ C ^ D) + E + W[t] + K[3];
        E = D;
        D = C;
        C = SHA1CircularShift(30, B);
        B = A;
        A = temp;
    }

    Intermediate_Hash[0] += A;
    Intermediate_Hash[1] += B;
    Intermediate_Hash[2] += C;
    Intermediate_Hash[3] += D;
    Intermediate_Hash[4] += E;

    Message_Block_Index = 0;
}


/*
 *  SHA1Context::PadMessage
 *
 *  Description:
 *      According to the standard, the message must be padded to an even
 *      512 bits. The first padding bit must be a '1'. The last 64
 *      bits represent the length of the original message. All bits in
 *      between should be 0. This function will pad the message
 *      according to those rules by filling the Message_Block array
 *      accordingly. It will also call the ProcessMessageBlock function
 *      appropriately. When it returns, it can be assumed that
 *      the message digest has been computed.
 */
void SHA1Context::PadMessage() noexcept
{
    /*
     *  Check to see if the current message block is too small to hold
     *  the initial padding bits and length. If so, we will pad the
     *  block, process it, and then continue padding into a second
     *  block.
     */
    if (Message_Block_Index > 55)
    {
        Message_Block[Message_Block_Index++] = 0x80;
        while (Message_Block_Index < static_cast<int16_t>(SHA1MessageBlockSize))
        {
            Message_Block[Message_Block_Index++] = 0;
        }

        ProcessMessageBlock();

        while (Message_Block_Index < 56)
        {
            Message_Block[Message_Block_Index++] = 0;
        }
    }
    else
    {
        Message_Block[Message_Block_Index++] = 0x80;
        while (Message_Block_Index < 56)
        {
            Message_Block[Message_Block_Index++] = 0;
        }
    }

    /*
     *  Store the message length as the last 8 octets
     */
    Message_Block[56] = static_cast<uint8_t>(Length_High >> 24);
    Message_Block[57] = static_cast<uint8_t>(Length_High >> 16);
    Message_Block[58] = static_cast<uint8_t>(Length_High >> 8);
    Message_Block[59] = static_cast<uint8_t>(Length_High);
    Message_Block[60] = static_cast<uint8_t>(Length_Low >> 24);
    Message_Block[61] = static_cast<uint8_t>(Length_Low >> 16);
    Message_Block[62] = static_cast<uint8_t>(Length_Low >> 8);
    Message_Block[63] = static_cast<uint8_t>(Length_Low);

    ProcessMessageBlock();
}

/*
 *  C-style wrapper functions for backward compatibility
 */

extern "C" {
    int SHA1Reset(SHA1Context* context) noexcept
    {
        if (!context)
        {
            return shaNull;
        }
        return context->Reset();
    }

    int SHA1Input(SHA1Context* context, const uint8_t* message_array, unsigned int length) noexcept
    {
        if (!context)
        {
            return shaNull;
        }
        return context->Input(message_array, length);
    }

    int SHA1Result(SHA1Context* context, uint8_t Message_Digest[SHA1HashSize]) noexcept
    {
        if (!context)
        {
            return shaNull;
        }
        return context->Result(Message_Digest);
    }
}

