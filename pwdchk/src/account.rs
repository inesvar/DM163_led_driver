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
}
