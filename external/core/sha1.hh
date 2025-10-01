/*
 *  sha1.h
 *
 *  Description:
 *      This is the header file for code which implements the Secure
 *      Hashing Algorithm 1 as defined in FIPS PUB 180-1 published
 *      April 17, 1995.
 *
 *      Many of the variable names in this code, especially the
 *      single character names, were used because those were the names
 *      used in the publication.
 *
 *      Please read the file sha1.c for more information.
 *
 */

#ifndef _SHA1_H_
#define _SHA1_H_

#include <cstdint>
#include <array>
#include <cstring>

#ifndef _SHA_enum_
#define _SHA_enum_
enum SHA1ErrorCode : int
{
    shaSuccess = 0,
    shaNull,            /* Null pointer parameter */
    shaInputTooLong,    /* input data too long */
    shaStateError       /* called Input after Result */
};
#endif

constexpr size_t SHA1HashSize = 20;
constexpr size_t SHA1HashWords = SHA1HashSize / 4;
constexpr size_t SHA1MessageBlockSize = 64;

/*
 *  Modern C++ class for SHA-1 hashing operations
 */
class SHA1Context
{
public:
    // Constructor
    SHA1Context() noexcept;
    
    // Deleted copy operations for safety
    SHA1Context(const SHA1Context&) = delete;
    SHA1Context& operator=(const SHA1Context&) = delete;
    
    // Move operations
    SHA1Context(SHA1Context&&) noexcept = default;
    SHA1Context& operator=(SHA1Context&&) noexcept = default;
    
    // Core operations
    [[nodiscard]] int Reset() noexcept;
    [[nodiscard]] int Input(const uint8_t* message_array, unsigned int length) noexcept;
    [[nodiscard]] int Result(uint8_t* Message_Digest) noexcept;

private:
    std::array<uint32_t, SHA1HashWords> Intermediate_Hash;
    uint32_t Length_Low{0};
    uint32_t Length_High{0};
    int16_t Message_Block_Index{0};
    std::array<uint8_t, SHA1MessageBlockSize> Message_Block;
    bool Computed{false};
    int Corrupted{0};
    
    void PadMessage() noexcept;
    void ProcessMessageBlock() noexcept;
};

/*
 *  C-style wrapper functions for backward compatibility
 */
extern "C" {
    int SHA1Reset(SHA1Context* context) noexcept;
    int SHA1Input(SHA1Context* context, const uint8_t* message_array, unsigned int length) noexcept;
    int SHA1Result(SHA1Context* context, uint8_t Message_Digest[SHA1HashSize]) noexcept;
}

#endif

