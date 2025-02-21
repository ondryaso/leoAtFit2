-- InputIO.hs
-- FLP Project 1: ECDSA
-- Author: Ondřej Ondryáš <xondry02@stud.fit.vut.cz>
-- Year: 2023
--
-- This module contains functions for parsing program input.

{-# OPTIONS_GHC -Wno-unused-do-bind #-}

module InputIO
  ( parseCurve,
    parseKey,
    parseSignature,
    parsePublicKey,
    parseHash,
    parseSigRequest,
    parseVerifyRequest,
  )
where

import ECTypes
  ( Curve (Curve),
    Hash,
    Key (Key),
    Point (Point),
    PublicKey,
    Signature (Signature),
    fromSEC,
  )
import Numeric (readDec, readHex)
import Text.Parsec
  ( Parsec,
    char,
    digit,
    hexDigit,
    many1,
    newline,
    spaces,
    string, option, choice,
  )

parseCurve :: Parsec String () Curve
parseCurve = do
  string "Curve {"
  newline
  p <- parseHexNum "p"
  a <- parseNum "a"
  b <- parseNum "b"
  gPoint <- parsePoint "g"
  n <- parseHexNum "n"
  h <- parseNum "h"
  char '}'
  newline
  return $ Curve p a b gPoint n h

parseKey :: Parsec String () Key
parseKey = do
  string "Key {"
  newline
  d <- parseHexNum "d"
  (_:qstr) <- parseHexString "Q"
  char '}'
  newline
  case fromSEC qstr of
    Left x -> fail x
    Right k -> return $ Key d k

-- A hexa string parser.
-- Input: [+-]?0x[0-9a-fA-F]+
-- Output: [+-][0-9a-fA-F]+
parseHexString :: String -> Parsec String () String
parseHexString prop = do
  string prop
  char ':'
  spaces
  sign <- option '+' $ choice [char '+', char '-']
  string "0x"
  hexStr <- many1 hexDigit
  newline
  return $ sign : hexStr

parseSignature :: Parsec String () Signature
parseSignature = do
  string "Signature {"
  newline
  r <- parseHexNum "r"
  s <- parseHexNum "s"
  char '}'
  newline
  return $ Signature r s

parsePublicKey :: Parsec String () PublicKey
parsePublicKey = do
  string "PublicKey {"
  newline
  (_:qstr) <- parseHexString "Q"
  char '}'
  newline
  case fromSEC qstr of
    Left x -> fail x
    Right k -> return k

parseHash :: Parsec String () Hash
parseHash = parseHexNum "Hash"

parsePoint :: String -> Parsec String () Point
parsePoint prop = do
  string prop
  char ':'
  spaces
  string "Point {"
  newline
  spaces
  x <- parseHexNum "x"
  spaces
  y <- parseHexNum "y"
  char '}'
  newline
  return $ Point x y

-- Parses a hex number string with required leading sign.
parseHexNum :: (Num a, Eq a) => String -> Parsec String () a
parseHexNum prop = do
  (sign:hexStr) <- parseHexString prop
  case readHex hexStr of
    [(val, _)] -> return $ if sign == '-' then negate val else val
    _ -> fail $ prop ++ ": invalid value"

parseNum :: (Num a, Eq a) => String -> Parsec String () a
parseNum prop = do
  string prop
  char ':'
  spaces
  neg <- option '+' $ choice [char '+', char '-']
  numStr <- many1 digit
  newline
  case readDec numStr of
    [(val, _)] -> return $ if neg == '-' then negate val else val
    _ -> fail $ prop ++ ": invalid value"

parseSigRequest :: Parsec String () (Curve, Key, Hash)
parseSigRequest = (,,) <$> parseCurve <*> parseKey <*> parseHash

parseVerifyRequest :: Parsec String () (Curve, Signature, PublicKey, Hash)
parseVerifyRequest = (,,,) <$> parseCurve <*> parseSignature <*> parsePublicKey <*> parseHash