use std::collections::HashMap;
use std::str::FromStr;

#[derive(Debug)]
#[allow(dead_code)]
pub struct Account {
    login: String,
    password: String,
}

#[derive(Debug)]
pub struct NoColon;

pub fn group(accounts: Vec<Account>) -> HashMap<String, Vec<String>> {
    let mut logins_by_password = HashMap::<_, Vec<String>>::new();
    for account in accounts {
        logins_by_password
            .entry(account.password)
            .and_modify(|vec| vec.push(account.login.clone()))
            .or_insert(vec![account.login]);
    }
    logins_by_password.retain(|_key, value| value.len() > 1_usize);
    logins_by_password
}

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
