use crate::account::Account;
use rayon::prelude::*;
use sha1::{Digest, Sha1};
use std::time::Instant;

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
        //.iter()
        .map(|account| (sha1(account).0, sha1(account).1, account))
        .collect()
}

pub fn all_sha1_timed(account: &[Account]) -> Vec<(String, String, &Account)> {
    let start = Instant::now();
    let all_sha1 = all_sha1(account);
    let duration = start.elapsed();
    println!("Execution time of all_sha1 : {:?}", duration);
    all_sha1
}
