const { apiKey } = require('./config.json');
const openai = require('openai');
const client = new openai(apiKey);

const prompt = 'O que é o universo?';

openai.apiKey = apiKey;

openai.Completion.create({
  engine: 'davinci',
  prompt: prompt,
  maxTokens: 2048,
  n: 1,
  stop: '\n',
}).then((response) => {
  console.log(response.choices[0].text);
}).catch((error) => {
  console.log(error);
});
