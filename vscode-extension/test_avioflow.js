import avioflow from 'avioflow';
const decoder = new avioflow.AudioDecoder();
console.log('Methods:', Object.getOwnPropertyNames(Object.getPrototypeOf(decoder)));
