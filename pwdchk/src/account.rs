use std::str::FromStr;

#[derive(Debug)]
#[allow(dead_code)]
pub struct Account {
    login: String,
    password: String,
}

#[derive(Debug)]
pub struct NoColon;

impl FromStr for Account {
    type Err = NoColon;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s.split_once(':') {
            Some((login, password)) => Ok(Self::new(login, password)),
            None => Err(NoColon),
        }
    }
}

impl Account {
    pub fn new(login: &str, password: &str) -> Self {
        Self {
            login: login.to_owned(),
            password: password.to_owned(),
        }
    }
}
