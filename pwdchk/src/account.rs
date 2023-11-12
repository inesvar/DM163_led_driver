#[derive(Debug)]
#[allow(dead_code)]
pub struct Account {
    login: String,
    password: String,
}

impl Account {
    pub fn new(login: &str, password: &str) -> Self {
        Self {
            login: login.to_owned(),
            password: password.to_owned(),
        }
    }

    pub fn from_string(s: &str) -> Self {
        let (login, password) = s.split_once(':').unwrap();
        Self::new(login, password)
    }
}
