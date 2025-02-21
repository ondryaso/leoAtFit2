-- EllipticCurve.hs
-- FLP Project 1: ECDSA
-- Author: Ondřej Ondryáš <xondry02@stud.fit.vut.cz>
-- Year: 2023
--
-- This module contains supplementary functions for performing calculations
-- on elliptic curves.

module EllipticCurve (isSingular, onCurve, ecSum, ecDouble, ecScalarMul, modMulInv) where

import ECTypes (Curve (..), Point (..))

{-
curve: y² = x³ + ax + b

addition (mod p):

s   = (y_P - y_Q) * (x_P - x_Q)^-1   mod p
x_R = s * s - x_P - x_Q   mod p
y_R = s * (x_P - x_R) - y_P   mod p

doubling (mod p):

s   = (3 * x_P * x_P + a) * (2 * y_P)^-1   mod p
x_R = s * s - 2 * x_P   mod p
y_R = s * (x_P - x_R) - y_P   mod p

-}

-- | Determines if a curve is singular.
--
-- A curve is signular (cannot form a group) iff 4a³ + 27b² mod p == 0.
isSingular :: Curve -> Bool
isSingular c = (4 * (getA c ^ (3 :: Integer)) + 27 * (getB c ^ (2 :: Integer))) `mod` getP c == 0

-- | Determines if a point lies on a given curve.
onCurve :: Curve -> Point -> Bool
onCurve _ Inf = False
onCurve (Curve p a b _ _ _) (Point x y) =
  ((y ^ (2 :: Integer)) `mod` p) == ((x ^ (3 :: Integer) + a * x + b) `mod` p)

-- | Computes an elliptic curve point addition.
ecSum :: Curve -> Point -> Point -> Point
ecSum _ p Inf = p
ecSum _ Inf q = q
ecSum c (Point xp yp) (Point xq yq)
  | xp == xq = Inf
  | otherwise =
      let m = getP c -- modulus
          s = ((yp - yq) * modMulInv (xp - xq) m) `mod` m
          xr = (s * s - xp - xq) `mod` m
          yr = (s * (xp - xr) - yp) `mod` m
       in Point xr yr

-- | Computes an elliptic curve point doubling.
ecDouble :: Curve -> Point -> Point
ecDouble _ Inf = Inf
ecDouble c (Point xp yp) =
  let m = getP c
      a = getA c
      s = ((3 * xp * xp + a) * modMulInv (2 * yp) m) `mod` m
      xr = (s * s - 2 * xp) `mod` m
      yr = (s * (xp - xr) - yp) `mod` m
   in Point xr yr

-- | Computes an elliptic curve point multiplication by scalar.
--
-- Uses the 'double-and-add' method.
ecScalarMul :: Curve -> Integer -> Point -> Point
ecScalarMul _ 0 _ = Point 0 0
ecScalarMul _ 1 p = p
ecScalarMul c d p
  | d `mod` 2 == 1 = ecSum c p $ ecScalarMul c (d - 1) p
  | otherwise = ecScalarMul c (d `div` 2) $ ecDouble c p

-- | Computes the modular multiplicative inverse of @a@ with respect to a given @modulus@
-- using the extended Euclidean method.
modMulInv :: Integer -> Integer -> Integer
modMulInv a modulus
  | gcd a modulus /= 1 = error "a and modulus must be coprime"
  | otherwise =
      let (i, _) = extendedEuclidean a modulus
       in i `mod` modulus
  where
    extendedEuclidean _ 0 = (1, 0)
    extendedEuclidean c d =
      let (q, r) = divMod c d
          (s, t) = extendedEuclidean d r
       in (t, s - q * t)