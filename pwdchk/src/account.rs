use crate::error::Error;
use eyre::Result;
use std::collections::HashMap;
use std::fs::File;
use std::io::{BufRead, BufReader};
use std::path::Path;
use std::str::FromStr;

#[derive(Debug)]
#[allow(dead_code)]
pub struct Account {
    login: String,
    password: String,
}

pub fn group(accounts: &[Account]) -> HashMap<&str, Vec<&str>> {
    let mut logins_by_password = HashMap::<_, Vec<&str>>::new();
    for account in accounts {
        logins_by_password
            .entry(account.password.as_str())
            .and_modify(|vec| vec.push(&account.login))
            .or_insert(vec![&account.login]);
    }
    logins_by_password.retain(|_key, value| value.len() > 1_usize);
    logins_by_password
}

impl FromStr for Account {
    type Err = Error;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s.split_once(':') {
            Some((login, password)) => Ok(Self::new(login, password)),
            None => Err(Error::NoColon),
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

    pub fn from_file(filename: &Path) -> Result<Vec<Account>> {
        let file = File::open(filename)?;
        let reader = BufReader::new(file);
        Ok(reader
            .lines()
            .map(|s| Account::from_str(&s?))
            .collect::<Result<Vec<_>, _>>()?)
    }
}
