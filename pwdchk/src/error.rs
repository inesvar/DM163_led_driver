use std::fmt::Display;
use std::fmt::Formatter;

#[derive(Debug)]
pub enum Error {
    #[allow(clippy::enum_variant_names)]
    IoError(std::io::Error),
    NoColon,
    ParseInt(std::num::ParseIntError),
    Reqwest(reqwest::Error),
}

impl From<std::io::Error> for Error {
    fn from(io_error: std::io::Error) -> Self {
        Error::IoError(io_error)
    }
}

impl From<reqwest::Error> for Error {
    fn from(reqwest_error: reqwest::Error) -> Self {
        Error::Reqwest(reqwest_error)
    }
}

impl From<std::num::ParseIntError> for Error {
    fn from(parse_int_error: std::num::ParseIntError) -> Self {
        Error::ParseInt(parse_int_error)
    }
}

impl Display for Error {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), std::fmt::Error> {
        match self {
            Error::NoColon => write!(f, "No colon found"),
            _ => write!(f, "{:?}", self),
        }
    }
}

impl std::error::Error for Error {}
