-- ECTypes.hs
-- FLP Project 1: ECDSA
-- Author: Ondřej Ondryáš <xondry02@stud.fit.vut.cz>
-- Year: 2023
--
-- This module contains definitions of data structures used to represent
-- the cryptographic primitives internally. It also defines functions used
-- to convert them to the required text representations.

module ECTypes
  ( Point (..),
    Curve (..),
    Key (..),
    Signature (..),
    PublicKey,
    PrivateKey,
    Hash,
    fromSEC,
    toSEC,
    showKey,
  )
where

import Data.Char (isHexDigit, toUpper)
import Data.List (foldl')
import GHC.Num (integerLog2)
import Numeric (readHex, showHex)

-- | A point on an elliptic curve.
-- Exports two constructors: @Inf@ that represents a point in infinity
-- (the identity element); and @Point x y@ which represents a specific point.
data Point = Inf | Point Integer Integer
  deriving (Eq)

-- | An elliptic curve.
-- Exports a single constructor, @Curve p a b G n h@, where @p@ is the prime modulus,
-- @a@ and @b@ are the curve's parameters, @G@ is the curve's base point, @n@ is the
-- integer order of G (nG = identity) and @h@ is the cofactor.
data Curve = Curve
  { getP :: Integer, -- EC field modulus
    getA :: Integer, -- EC parameter A
    getB :: Integer, -- EC parameter B
    getG :: Point, -- point on the EC
    getN :: Integer, -- int order of G (n * G = 0), must be prime
    getH :: Integer -- EC cofactor
  }

-- | An ECDSA key pair.
-- Exports a single constructor, @Key d q@, where @d@ is the private key and @q@ is
-- the corresponding public key.
data Key = Key
  { getKeyD :: PrivateKey,
    getKeyQ :: PublicKey
  }

-- | An ECDSA digital signature.
-- Exports a single constructor, @Signature r s@, where @r@ and @s@ are the signature's parameters.
data Signature = Signature
  { getSigR :: Integer,
    getSigS :: Integer
  }

-- | An ECDSA public key.
type PublicKey = Point

-- | An ECDS private key.
type PrivateKey = Integer

-- | A hash.
type Hash = Integer

showCurve :: Curve -> String
showCurve c =
  "Curve {\np: "
    ++ showTargetBigHex (getP c)
    ++ "\na: "
    ++ show (getA c)
    ++ "\nb: "
    ++ show (getB c)
    ++ "\ng: "
    ++ showPoint (getG c)
    ++ "\nn: "
    ++ showTargetBigHex (getN c)
    ++ "\nh: "
    ++ show (getH c)
    ++ "\n}\n"

showPoint :: Point -> String
showPoint Inf =
  "Point {\n    x: 0x00\n    y: 0x00\n}"
showPoint (Point x y) =
  "Point {\n    x: "
    ++ showTargetBigHex x
    ++ "\n    y: "
    ++ showTargetBigHex y
    ++ "\n}"

showKey :: Curve -> Key -> String
showKey c k =
  "Key {\nd: "
    ++ showTargetHex (getKeyD k)
    ++ "\nQ: 0x"
    ++ toSEC (getP c) (getKeyQ k)
    ++ "\n}\n"

showSignature :: Signature -> String
showSignature s =
  "Signature {\nr: "
    ++ showTargetHex (getSigR s)
    ++ "\ns: "
    ++ showTargetHex (getSigS s)
    ++ "\n}\n"

showTargetHex :: Integer -> String
showTargetHex x
  | x < 0 = "-0x" ++ showHex (negate x) ""
  | otherwise = "0x" ++ showHex x ""

showTargetBigHex :: Integer -> String
showTargetBigHex x
  | x < 0 = "-0x" ++ map toUpper (showHex (negate x) "")
  | otherwise = "0x" ++ map toUpper (showHex x "")

-- | Converts a @Point@ on an EC over field F_@p@ to the Uncompressed SEC format.
--
-- The length of an octet string that represents a coordinate of a point
-- on a curve over F_p must be contain ceil((log2 p) / 8) octets so double that
-- amount of HEXA DIGITS.
toSEC :: Integer -> Point -> String
toSEC _ Inf = error "Cannot convert infinity point to SEC format"
toSEC p (Point x y) = "04" ++ encode x ++ encode y
  where
    padToLength l xs = replicate (l - length xs) '0' ++ xs
    digitsNeeded = (* 2) . ceiling $ (fromIntegral (integerLog2 p) :: Double) / 8
    encode :: Integer -> String
    encode n = padToLength digitsNeeded $ showHex n ""

-- | Converts an Uncompressed SEC formatted key (point on a curve) to a @Point@.
fromSEC :: String -> Either String Point
fromSEC ('0' : '4' : xs) =
  -- don't mind me, I was just playing with applicatives here
  let l =
        foldl' -- get length and check format, returns Either (String error msg) (Int result)
          ( \acc x ->
              if isHexDigit x
                then (+ 1) <$> acc
                else Left "cannot parse key (invalid hex number)"
          )
          (Right (0 :: Int))
          xs
      -- check if the string contains a valid number of octets (two parts of same length, 
      -- each consists of hex digit pairs)
      lm = (`mod` 4) <$> l
      lh' = case lm of -- lh' is length of a single coordinate string (in hex digits)
        Left _ -> lm -- propagate error
        Right x -> if x == 0 then (`div` 2) <$> l else Left "cannot parse key (invalid data length)"
      xstr = readHex <$> (take <$> lh' <*> pure xs)
      ystr = readHex <$> (drop <$> lh' <*> pure xs)
   in Right Point <*> c xstr <*> c ystr
  where
    c (Left x) = Left x
    c (Right [(x, _)]) = Right x
    c (Right _) = Left "cannot parse key (invalid hex number)"
fromSEC _ = Left "key not in uncompressed SEC format"

instance Show Signature where
  show = showSignature

instance Show Curve where
  show = showCurve

instance Show Point where
  show = showPoint

-- used only for debugging purposes, not for program output,
-- as that requires converting the Q pubkey to the SEC format
-- which depends on the modulus of the used curve
-- according to SEC, 2.3.3 / 2.3.5
instance Show Key where
  show k =
    "Key {\nd: "
      ++ showTargetHex (getKeyD k)
      ++ "\nQ: "
      ++ showPoint (getKeyQ k)
      ++ "\n}\n"