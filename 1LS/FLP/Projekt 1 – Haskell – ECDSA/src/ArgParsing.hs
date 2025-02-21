-- ArgParsing.hs
-- FLP Project 1: ECDSA
-- Author: Ondřej Ondryáš <xondry02@stud.fit.vut.cz>
-- Year: 2023
--
-- This module contains definitions of types and functions used for parsing
-- the program's command-line arguments.

module ArgParsing (parse, Flags (..)) where

import Control.Monad (when)
import System.Console.GetOpt
  ( ArgDescr (NoArg),
    ArgOrder (Permute),
    OptDescr (..),
    getOpt,
    usageInfo,
  )
import System.Environment (getProgName)
import System.Exit (ExitCode (ExitFailure), exitWith)
import System.IO (hPutStrLn, stderr)

-- | Represents the possible command-line program mode options.
data Flags
  = PrintEC
  | GenerateKeys
  | GenerateSignature
  | CheckSignature
  deriving (Eq, Ord, Enum, Show, Bounded)

flags :: [OptDescr Flags]
flags =
  [ Option
      ['i']
      []
      (NoArg PrintEC)
      "Loads an EC configuration and prints it back.",
    Option
      ['k']
      []
      (NoArg GenerateKeys)
      "Loads an EC configuration and generates a key pair.",
    Option
      ['s']
      []
      (NoArg GenerateSignature)
      "Loads an EC configuration, a private key and a message digest. Signs the digest and prints out the signature.",
    Option
      ['v']
      []
      (NoArg CheckSignature)
      "Loads an EC configuration, a public key and a digital signature. Checks the signature."
  ]

-- | @parse argv@ parses the given list of command-line arguments and returns the specified
-- program mode flag and the input filename, if specified. In case of invalid arguments,
-- a corresponding error message is printed to stderr, @exitWith@ is called and returned.
parse :: [String] -> IO (Flags, Maybe String)
parse argv = case getOpt Permute flags argv of
  (args, file, []) -> do
    when (length args /= 1) $ parseError "Exactly one of i/k/s/v mode-specifying parameters must be used." 2
    when (length file > 1) $ parseError "Only one file can be provided." 3

    return (head args, if null file then Nothing else Just (head file))
  (_, _, errs) -> do
    parseError (unlines errs) 1

parseError :: [Char] -> Int -> IO b
parseError msg code = do
  exePath <- getProgName
  hPutStrLn stderr (msg ++ ['\n'] ++ usageInfo (header exePath) flags)
  exitWith (ExitFailure code)
  where
    header x = "Usage: " ++ x ++ " [-iksv] [file]"