/* eslint no-unused-expressions: 0 */
'use strict'

const fs = require('fs-extra')
const path = require('path')
const expect = require('chai').expect
const nroonga = require('../lib/nroonga')

/* global beforeEach, afterEach, describe, it */
describe('plugin', () => {
  let db = null
  const tempdir = path.join('test', 'tmp')
  const databaseName = `tempdb-${process.pid}-${(new Date()).valueOf()}`

  beforeEach(() => {
    if (!fs.existsSync(tempdir)) fs.mkdirSync(tempdir)
    db = new nroonga.Database(path.join(tempdir, databaseName))
    db.commandSync('plugin_register normalizers/mysql')
    db.commandSync('plugin_register ruby/eval')
  })

  afterEach(() => {
    db.close()
    fs.removeSync(tempdir)
  })

  describe('Normalizer MySQL', () => {
    it('should normalize haha', () => {
      const matched = db.commandSync('normalize NormalizerMySQLUnicode900 "はハ"')
      const expected = { 'normalized': 'はは', 'types': [], 'checks': [] }
      expect(matched).to.deep.equal(expected)
    })
  })

  describe('Ruby', () => {
    it('should evaluate ruby script', () => {
      const matched = db.commandSync('ruby_eval "1 + 2"')
      const expected = { 'value': 3 }
      expect(matched).to.deep.equal(expected)
    })
  })
})
