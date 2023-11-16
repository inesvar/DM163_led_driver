use std::fmt::Display;
use std::fmt::Formatter;

#[derive(Debug)]
pub enum Error {
    IoError(std::io::Error),
    NoColon,
}

impl From<std::io::Error> for Error {
    fn from(io_error: std::io::Error) -> Self {
        Error::IoError(io_error)
    }
}

impl Display for Error {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), std::fmt::Error> {
        match self {
            Error::IoError(io_error) => write!(f, "Error: IoError ({})", io_error),
            Error::NoColon => write!(f, "Error: NoColon"),
        }
    }
}

impl std::error::Error for Error {}
