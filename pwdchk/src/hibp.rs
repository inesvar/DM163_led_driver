use crate::account::Account;
use rayon::prelude::*;
use sha1::{Digest, Sha1};
use std::collections::HashMap;

fn sha1(account: &Account) -> (String, String) {
    let mut hasher = Sha1::new();
    hasher.update(account.password.clone());
    let result = hasher.finalize();
    let mut sha1 = format!("{result:X}");
    let sha1_suffix = sha1.split_off(5);
    (sha1, sha1_suffix)
}

fn all_sha1(account: &[Account]) -> Vec<(String, String, &Account)> {
    account
        .par_iter()
        .map(|account| (sha1(account).0, sha1(account).1, account))
        .collect()
}

pub fn sha1_by_prefix(accounts: &[Account]) -> HashMap<String, Vec<(String, &Account)>> {
    let all_sha1 = all_sha1(accounts)
        .into_iter()
        .map(|(prefix, suffix, account)| (prefix, (suffix, account)));
    let mut accounts_by_sha1 = HashMap::<_, Vec<(_, &Account)>>::new();
    for (prefix, account) in all_sha1 {
        accounts_by_sha1
            .entry(prefix)
            .and_modify(|vec| vec.push(account.clone()))
            .or_insert(vec![account]);
    }
    accounts_by_sha1
}
