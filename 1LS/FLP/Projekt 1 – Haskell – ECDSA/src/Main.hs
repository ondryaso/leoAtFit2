-- Main.hs
-- FLP Project 1: ECDSA
-- Author: Ondřej Ondryáš <xondry02@stud.fit.vut.cz>
-- Year: 2023

module Main (main) where

import ArgParsing (Flags (..), parse)
import Data.Either (fromRight)
import Data.Maybe (fromMaybe)
import ECDSA (makeKeyPair, makeSignature, verifySignature)
import ECTypes (Key (getKeyD), showKey)
import InputIO (parseCurve, parseSigRequest, parseVerifyRequest)
import System.Environment (getArgs)
import System.Exit (ExitCode (ExitFailure, ExitSuccess), exitWith)
import System.IO (Handle, IOMode (ReadMode), hClose, hGetContents, hPutStrLn, openFile, stderr, stdin)
import System.Random (newStdGen)
import qualified Text.Parsec as P

printEC :: String -> Handle -> IO (Bool, String)
printEC fp handle = do
  contents <- hGetContents handle
  let curve = P.parse parseCurve fp contents
  return $ case curve of
    Left e -> (False, "Input error: " ++ show e)
    Right c -> (True, show c)

generateKeys :: String -> Handle -> IO (Bool, String)
generateKeys fp handle = do
  contents <- hGetContents handle
  let curve = P.parse parseCurve fp contents
  gen <- newStdGen
  let pair = makeKeyPair gen <$> curve
  return $ case pair of
    Left e -> (False, "Input error: " ++ show e)
    -- fromRight is safe here, curve cannot be Left ParseError if pair isn't Left as well
    Right (key, _) -> (True, showKey (fromRight undefined curve) key)

generateSignature :: String -> Handle -> IO (Bool, String)
generateSignature fp handle = do
  contents <- hGetContents handle
  let parseRes = P.parse parseSigRequest fp contents
  gen <- newStdGen
  -- I'm not convinced this is a proper way of propagating errors in a Left but w/e, it works
  let sig = parseRes >>= (\(c, k, h) -> Right $ makeSignature gen c (getKeyD k) h)
  return $ case sig of
    Left e -> (False, "Input error: " ++ show e)
    Right (s, _) -> (True, show s)

checkSignature :: String -> Handle -> IO (Bool, String)
checkSignature fp handle = do
  contents <- hGetContents handle
  let parseRes = P.parse parseVerifyRequest fp contents
  let sig = parseRes >>= (\(c, s, k, h) -> Right $ verifySignature c s k h)
  return $ case sig of
    Left e -> (False, "Input error: " ++ show e)
    Right b -> (True, show b ++ "\n")

main :: IO ()
main = do
  (flag, file) <- getArgs >>= parse
  handle <- case file of
    Nothing -> return stdin
    Just fn -> openFile fn ReadMode
  let fn = fromMaybe "stdin" file
  let nextfun = case flag of
        PrintEC -> printEC
        GenerateKeys -> generateKeys
        GenerateSignature -> generateSignature
        CheckSignature -> checkSignature
  (res, out) <- nextfun fn handle
  rc <-
    if res
      then (putStr $! out) >> hClose handle >> return ExitSuccess
      else (hPutStrLn stderr $! out) >> hClose handle >> return (ExitFailure 1)
  exitWith rc
