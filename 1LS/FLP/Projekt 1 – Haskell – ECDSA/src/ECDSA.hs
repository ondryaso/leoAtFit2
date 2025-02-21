-- ECDSA.hs
-- FLP Project 1: ECDSA
-- Author: Ondřej Ondryáš <xondry02@stud.fit.vut.cz>
-- Year: 2023
--
-- This module exports functions used to perform ECDSA operations such
-- as key generation.

module ECDSA (makePubKey, makeKeyPair, makeSignature, verifySignature) where

import Data.Bits (Bits (shiftR), complement, shiftL, (.&.))
import ECTypes (Curve (..), Hash, Key (..), Point (..), PrivateKey, PublicKey, Signature (..))
import EllipticCurve (ecScalarMul, ecSum, modMulInv, onCurve)
import GHC.Num (integerLog2)
import System.Random (RandomGen, uniformR)

-- | @makePubKey c d@ returns a public key corresponding to
-- a private key @d@ on a given EC @c@.
makePubKey :: Curve -> PrivateKey -> PublicKey
makePubKey c d = ecScalarMul c d (getG c)

-- | @makeKeyPair gen c@ generates a new keypair on a given EC @c@.
-- @gen@ is a random number generator that should be, preferrably,
-- cryptographically strong.
--
-- A tuple of @(k, g)@ is returned where @k@ is the keypair and @g@ is a new instance
-- of the provided PRNG.
makeKeyPair :: (RandomGen g) => g -> Curve -> (Key, g)
makeKeyPair gen c =
  let (d, gen') = uniformR (1, getN c - 1) gen
      qa = makePubKey c d
   in (Key d qa, gen')

-- | @makeSignature gen c d h@ returns a digital signature made using a given EC @c@,
-- private key @d@ for a message with hash @h@. @gen@ is a random number generator that
-- should be, preferrably, cryptographically strong.
--
-- A tuple of @(s, g)@ is returned where @s@ is the signature and @g@ is a new instance
-- of the provided PRNG.
--
-- The bit length of the hash must be equal or greater than the bit length of the curve's
-- @n@ parameter, otherwise exectuion is aborted with an error.
makeSignature :: (RandomGen g) => g -> Curve -> PrivateKey -> Hash -> (Signature, g)
makeSignature gen c d h =
  let n = getN c
      z = getLeftmostBits n h
      (k, gen') = uniformR (1, getN c - 1) gen
      k' = modMulInv k n
      p = ecScalarMul c k (getG c)
      -- extract point's x as Just Integer, or Nothing if point is Inf
      r = getRMaybe n p
      -- apply the Maybe Point into the calculation of s
      s = (\r' -> (k' * ((z + (r' * d)) `mod` n)) `mod` n) <$> r
      -- and create a Maybe Signature
      res = Signature <$> r <*> s
   in case res of
        Nothing -> makeSignature gen' c k h
        Just (Signature 0 _) -> makeSignature gen' c k h
        Just (Signature _ 0) -> makeSignature gen' c k h
        Just x -> (x, gen')
  where
    getRMaybe _ Inf = Nothing
    getRMaybe n (Point x _) = Just $ x `mod` n

-- | @publicKeyValid c k@ checks if a public key point @k@ is valid for a given EC @c@,
-- that is, @k@ is not identity, lies on the EC and n@k@ == identity.
publicKeyValid :: Curve -> PublicKey -> Bool
publicKeyValid c k =
  k /= Inf
    && onCurve c k
    && ecScalarMul c (getN c) k == Inf

-- | @verifySignature c s q h@ verifies a digital signature @s@ of a message
-- with hash @h@ using a public key @q@ and a given EC @c@.
--
-- The bit length of the hash must be equal or greater than the bit length of the curve's
-- @n@ parameter, otherwise exectuion is aborted with an error.
verifySignature :: Curve -> Signature -> PublicKey -> Hash -> Bool
verifySignature c (Signature r s) q h =
  let valid = publicKeyValid c q
      n = getN c
      z = getLeftmostBits n h
      s' = modMulInv s n
      r' = r `mod` n
      u1 = (z * s') `mod` n
      u2 = (r' * s') `mod` n
      u1' = ecScalarMul c u1 (getG c)
      u2' = ecScalarMul c u2 q
      u = ecSum c u1' u2'
   in valid && case u of
        Inf -> False
        (Point x _) -> r' == x `mod` n

-- | @getLeftmostBits n h@ returns the first L leftmost bits of @h@ where
-- L is the bit length of @n@.
--
-- The bitlength of the input hash SHOULD be higher than the bitlength of the
-- curve's prime order @n@. However, as this program has no knowledge of the
-- semantics of the provided hash value, it cannot decide whether the hash
-- actually satisfies this requirement if it has leading zero bits. For this
-- reason, shorter hashes are passed as-is.
getLeftmostBits :: Integer -> Integer -> Integer
getLeftmostBits n h =
  get (bitLength n) (bitLength h)
  where
    get nlen hlen
      | hlen > nlen =
          let shiftAm = hlen - nlen
              mask = complement $ complement (0 :: Integer) `shiftL` nlen
           in (h `shiftR` shiftAm) .&. mask
      | otherwise = h

-- | @bitLength x@ returns the number of bits needed to store the Integer @x@.
bitLength :: Integer -> Int
bitLength x = 1 + fromIntegral (integerLog2 x)